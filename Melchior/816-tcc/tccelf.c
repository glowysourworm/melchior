/*
 *  ELF file handling for TCC
 *
 *  Copyright (c) 2001-2004 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef TCC_TARGET_X86_64
#define ElfW_Rel ElfW(Rela)
#define SHT_RELX SHT_RELA
#define REL_SECTION_FMT ".rela%s"
/* x86-64 requires PLT for DLLs */
#define TCC_OUTPUT_DLL_WITH_PLT
#else
#define ElfW_Rel ElfW(Rel)
#define SHT_RELX SHT_REL
#define REL_SECTION_FMT ".rel%s"
#endif

/* XXX: DLL with PLT would only work with x86-64 for now */
//#define TCC_OUTPUT_DLL_WITH_PLT

/**
 * @brief Write a string to an ELF section.
 *
 * This function writes a null-terminated string `sym` to the ELF section `s`.
 * It returns the offset in the section where the string is written.
 *
 * @param s The ELF section to write the string to.
 * @param sym The null-terminated string to write.
 *
 * @return The offset in the section where the string is written.
 */
static int put_elf_str(Section *s, const char *sym)
{
    int offset, len;
    char *ptr;

    len = strlen(sym) + 1;
    offset = s->data_offset;
    ptr = section_ptr_add(s, len);
    memcpy(ptr, sym, len);
    return offset;
}

/**
 * @brief Compute the ELF hash value for a given name.
 *
 * This function computes the ELF hash value for the given null-terminated string `name`.
 * It iterates over each character of the string, performing bitwise operations to update
 * the hash value. The computed hash value is returned.
 *
 * @param name The null-terminated string for which to compute the ELF hash.
 *
 * @return The computed ELF hash value.
 */
static unsigned long elf_hash(const unsigned char *name)
{
    unsigned long h = 0, g;

    while (*name) {
        h = (h << 4) + *name++;
        g = h & 0xf0000000;
        if (g)
            h ^= g >> 24;
        h &= ~g;
    }
    return h;
}

/**
 * @brief Rebuild the hash table for a section.
 *
 * This function rebuilds the hash table for the given section `s` with the specified
 * number of buckets `nb_buckets`. It updates the hash table data, bucket counts, and
 * symbol indices based on the section's data and string table. The rebuilt hash table
 * is stored in the section's `hash` member.
 *
 * @param s The section for which to rebuild the hash table.
 * @param nb_buckets The number of buckets in the hash table.
 */
static void rebuild_hash(Section *s, unsigned int nb_buckets)
{
    ElfW(TokenSym) * sym;
    int *ptr, *hash, nb_syms, sym_index, h;
    char *strtab;

    strtab = s->link->data;
    nb_syms = s->data_offset / sizeof(ElfW(TokenSym));

    s->hash->data_offset = 0;
    ptr = section_ptr_add(s->hash, (2 + nb_buckets + nb_syms) * sizeof(int));
    ptr[0] = nb_buckets;
    ptr[1] = nb_syms;
    ptr += 2;
    hash = ptr;
    memset(hash, 0, (nb_buckets + 1) * sizeof(int));
    ptr += nb_buckets + 1;

    sym = (ElfW(TokenSym) *) s->data + 1;
    for (sym_index = 1; sym_index < nb_syms; sym_index++) {
        if (ELFW(ST_BIND)(sym->st_info) != STB_LOCAL) {
            h = elf_hash(strtab + sym->st_name) % nb_buckets;
            *ptr = hash[h];
            hash[h] = sym_index;
        } else {
            *ptr = 0;
        }
        ptr++;
        sym++;
    }
}

/**
 * @brief Write an ELF symbol entry to a section.
 *
 * This function writes an ELF symbol entry to the given section `s` with the specified
 * `value`, `size`, `info`, `other`, `shndx`, and `name`. It computes the offset for the
 * symbol's name and sets the values of the ELF symbol structure accordingly. The function
 * also updates the corresponding hash table entry if the section has a hash table.
 *
 * @param s The section to write the ELF symbol entry to.
 * @param value The value of the symbol.
 * @param size The size of the symbol.
 * @param info The symbol information.
 * @param other The symbol visibility and other attributes.
 * @param shndx The section index of the symbol.
 * @param name The name of the symbol.
 *
 * @return The index of the written symbol in the section's data.
 */
static int put_elf_sym(Section *s,
                       unsigned long value,
                       unsigned long size,
                       int info,
                       int other,
                       int shndx,
                       const char *name)
{
    int name_offset, sym_index;
    int nbuckets, h;
    ElfW(TokenSym) * sym;
    Section *hs;

    sym = section_ptr_add(s, sizeof(ElfW(TokenSym)));
    if (name)
        name_offset = put_elf_str(s->link, name);
    else
        name_offset = 0;
    /* XXX: endianness */
    sym->st_name = name_offset;
    sym->st_value = value;
    sym->st_size = size;
    sym->st_info = info;
    sym->st_other = other;
    sym->st_shndx = shndx;
    sym_index = sym - (ElfW(TokenSym) *) s->data;
    hs = s->hash;
    if (hs) {
        int *ptr, *base;
        ptr = section_ptr_add(hs, sizeof(int));
        base = (int *) hs->data;
        /* only add global or weak symbols */
        if (ELFW(ST_BIND)(info) != STB_LOCAL) {
            /* add another hashing entry */
            nbuckets = base[0];
            h = elf_hash(name) % nbuckets;
            *ptr = base[2 + h];
            base[2 + h] = sym_index;
            base[1]++;
            /* we resize the hash table */
            hs->nb_hashed_syms++;
            if (hs->nb_hashed_syms > 2 * nbuckets) {
                rebuild_hash(s, 2 * nbuckets);
            }
        } else {
            *ptr = 0;
            base[1]++;
        }
    }
    return sym_index;
}

/**
 * @brief Find an ELF symbol index by name in a section.
 *
 * This function searches for an ELF symbol with the given `name` in the section `s`.
 * If the section has a hash table, it uses the ELF hash algorithm to determine the
 * symbol's hash bucket and iterates through the linked list of symbols in that bucket
 * to find a matching symbol by name. The function returns the index of the matching
 * symbol in the section's data, or 0 if no match is found.
 *
 * @param s The section to search for the symbol.
 * @param name The name of the symbol to find.
 *
 * @return The index of the matching symbol, or 0 if not found.
 */
static int find_elf_sym(Section *s, const char *name)
{
    ElfW(TokenSym) * sym;
    Section *hs;
    int nbuckets, sym_index, h;
    const char *name1;

    hs = s->hash;
    if (!hs)
        return 0;
    nbuckets = ((int *) hs->data)[0];
    h = elf_hash(name) % nbuckets;
    sym_index = ((int *) hs->data)[2 + h];
    while (sym_index != 0) {
        sym = &((ElfW(TokenSym) *) s->data)[sym_index];
        name1 = s->link->data + sym->st_name;
        if (!strcmp(name, name1))
            return sym_index;
        sym_index = ((int *) hs->data)[2 + nbuckets + sym_index];
    }
    return 0;
}

/* return elf symbol value, signal error if 'err' is nonzero */
static void *get_elf_sym_addr(TCCState *s, const char *name, int err)
{
    int sym_index;
    ElfW(TokenSym) * sym;

    sym_index = find_elf_sym(s->symtab, name);
    sym = &((ElfW(TokenSym) *) s->symtab->data)[sym_index];
    if (!sym_index || sym->st_shndx == SHN_UNDEF) {
        if (err)
            error("%s not defined", name);
        return NULL;
    }
    return (void *) (uplong) sym->st_value;
}

#ifdef TCC_TARGET_816
/**
 * @brief Get an ELF symbol by name and update its value.
 *
 * This function searches for an ELF symbol with the given `name` in the symbol table
 * section `symtab_section` and updates the value pointed to by `pval` with the symbol's
 * value. If the symbol is found, a pointer to the symbol structure is returned. If the
 * symbol is not found, NULL is returned.
 *
 * @param s The TCC state.
 * @param pval Pointer to store the symbol's value.
 * @param name The name of the symbol to find.
 *
 * @return Pointer to the ELF symbol structure if found, or NULL if not found.
 */
ElfW(TokenSym) * tcc_really_get_symbol(TCCState *s, unsigned long *pval, const char *name)
{
    int sym_index;
    ElfW(TokenSym) * sym;

    sym_index = find_elf_sym(symtab_section, name);
    if (!sym_index)
        return NULL;
    sym = &((ElfW(TokenSym) *) symtab_section->data)[sym_index];
    *pval = sym->st_value;
    return sym;
}
#endif

/**
 * @brief Get the value of an ELF symbol by name.
 *
 * This function searches for an ELF symbol with the given `name` in the symbol table
 * section `symtab_section`. If the symbol is found, it returns a void pointer to the
 * symbol's value. If the symbol is not found, NULL is returned.
 *
 * @param s The TCC state.
 * @param name The name of the symbol to find.
 *
 * @return Void pointer to the symbol's value if found, or NULL if not found.
 */
void *tcc_get_symbol(TCCState *s, const char *name)
{
    return get_elf_sym_addr(s, name, 0);
}

/**
 * @brief Get the value of an ELF symbol by name with error handling.
 *
 * This function retrieves the value of an ELF symbol with the given `name` using
 * the `tcc_get_symbol` function. If the symbol is found, it returns a void pointer
 * to the symbol's value. If the symbol is not found, an error message is generated
 * using the `error` function and NULL is returned.
 *
 * @param s The TCC state.
 * @param name The name of the symbol to find.
 *
 * @return Void pointer to the symbol's value if found, or NULL if not found.
 */
void *tcc_get_symbol_err(TCCState *s, const char *name)
{
    return get_elf_sym_addr(s, name, 1);
}

/**
 * @brief Add an ELF symbol to a section.
 *
 * This function adds an ELF symbol to the given section `s` with the specified `value`,
 * `size`, `info`, `other`, `sh_num`, and `name`. It performs symbol lookup and resolution
 * to handle cases where a symbol is already defined. The function returns the index of the
 * added or patched symbol in the section's data.
 *
 * @param s The section to add the ELF symbol to.
 * @param value The value of the symbol.
 * @param size The size of the symbol.
 * @param info The symbol information.
 * @param other The symbol visibility and other attributes.
 * @param sh_num The section index of the symbol.
 * @param name The name of the symbol.
 *
 * @return The index of the added or patched symbol in the section's data.
 */
static int add_elf_sym(
    Section *s, uplong value, unsigned long size, int info, int other, int sh_num, const char *name)
{
    ElfW(TokenSym) * esym;
    int sym_bind, sym_index, sym_type, esym_bind;
    unsigned char sym_vis, esym_vis, new_vis;

    sym_bind = ELFW(ST_BIND)(info);
    sym_type = ELFW(ST_TYPE)(info);
    sym_vis = ELFW(ST_VISIBILITY)(other);

    if (sym_bind != STB_LOCAL) {
        /* we search global or weak symbols */
        sym_index = find_elf_sym(s, name);
        if (!sym_index)
            goto do_def;
        esym = &((ElfW(TokenSym) *) s->data)[sym_index];
        if (esym->st_shndx != SHN_UNDEF) {
            esym_bind = ELFW(ST_BIND)(esym->st_info);
            /* propagate the most constraining visibility */
            /* STV_DEFAULT(0)<STV_PROTECTED(3)<STV_HIDDEN(2)<STV_INTERNAL(1) */
            esym_vis = ELFW(ST_VISIBILITY)(esym->st_other);
            if (esym_vis == STV_DEFAULT) {
                new_vis = sym_vis;
            } else if (sym_vis == STV_DEFAULT) {
                new_vis = esym_vis;
            } else {
                new_vis = (esym_vis < sym_vis) ? esym_vis : sym_vis;
            }
            esym->st_other = (esym->st_other & ~ELFW(ST_VISIBILITY)(-1)) | new_vis;
            other = esym->st_other; /* in case we have to patch esym */
            if (sh_num == SHN_UNDEF) {
                /* ignore adding of undefined symbol if the
                   corresponding symbol is already defined */
            } else if (sym_bind == STB_GLOBAL && esym_bind == STB_WEAK) {
                /* global overrides weak, so patch */
                goto do_patch;
            } else if (sym_bind == STB_WEAK && esym_bind == STB_GLOBAL) {
                /* weak is ignored if already global */
            } else if (sym_vis == STV_HIDDEN || sym_vis == STV_INTERNAL) {
                /* ignore hidden symbols after */
            } else if (esym->st_shndx == SHN_COMMON
                       && (sh_num < SHN_LORESERVE || sh_num == SHN_COMMON)) {
                /* gr: Happens with 'tcc ... -static tcctest.c' on e.g. Ubuntu 6.01
                   No idea if this is the correct solution ... */
                goto do_patch;
            } else if (s == tcc_state->dynsymtab_section) {
                /* we accept that two DLL define the same symbol */
            } else {
#if 1
                printf("new_bind=%x new_shndx=%x new_vis=%x old_bind=%x old_shndx=%x old_vis=%x\n",
                       sym_bind,
                       sh_num,
                       new_vis,
                       esym_bind,
                       esym->st_shndx,
                       esym_vis);
#endif
                error_noabort("'%s' defined twice", name);
            }
        } else {
        do_patch:
            esym->st_info = ELFW(ST_INFO)(sym_bind, sym_type);
            esym->st_shndx = sh_num;
            esym->st_value = value;
            esym->st_size = size;
            esym->st_other = other;
        }
    } else {
    do_def:
        sym_index
            = put_elf_sym(s, value, size, ELFW(ST_INFO)(sym_bind, sym_type), other, sh_num, name);
    }
    return sym_index;
}

/**
 * @brief Add an ELF relocation entry to a relocation section.
 *
 * This function adds an ELF relocation entry to the given relocation section `symtab`
 * based on the provided `s` section, `offset`, `type`, and `symbol`. If the relocation
 * section does not exist, it is created. The function computes the relocation entry
 * values and adds the entry to the relocation section.
 *
 * @param symtab The symbol table section.
 * @param s The section associated with the relocation entry.
 * @param offset The offset within the section where the relocation applies.
 * @param type The relocation type.
 * @param symbol The symbol index to apply the relocation.
 */
static void put_elf_reloc(Section *symtab, Section *s, unsigned long offset, int type, int symbol)
{
    char buf[256];
    Section *sr;
    ElfW_Rel *rel;

    sr = s->reloc;
    if (!sr) {
        /* if no relocation section, create it */
        snprintf(buf, sizeof(buf), REL_SECTION_FMT, s->name);
        /* if the symtab is allocated, then we consider the relocation
           are also */
        sr = new_section(tcc_state, buf, SHT_RELX, symtab->sh_flags);
        sr->sh_entsize = sizeof(ElfW_Rel);
        sr->link = symtab;
        sr->sh_info = s->sh_num;
        s->reloc = sr;
    }
    rel = section_ptr_add(sr, sizeof(ElfW_Rel));
    rel->r_offset = offset;
    rel->r_info = ELFW(R_INFO)(symbol, type);
#ifdef TCC_TARGET_X86_64
    rel->r_addend = 0;
#endif
}

/**
 * @brief Debugging symbol entry for the Symbol Table (Stab) format.
 *
 * The `Stab_Sym` structure represents a debugging symbol entry in the Symbol Table
 * (Stab) format.
 */
typedef struct
{
    unsigned int n_strx;   /**< Index into the string table of the symbol's name. */
    unsigned char n_type;  /**< Type of the symbol. */
    unsigned char n_other; /**< Miscellaneous information about the symbol (usually empty). */
    unsigned short n_desc; /**< Description field for additional details about the symbol. */
    unsigned int n_value;  /**< Value associated with the symbol. */
} Stab_Sym;

/**
 * @brief Add a debugging symbol entry to the STABS section.
 *
 * This function adds a debugging symbol entry to the STABS section. It creates a new
 * `Stab_Sym` structure and sets its fields based on the provided parameters: `str`
 * (symbol name), `type` (symbol type), `other` (miscellaneous information), `desc`
 * (description), and `value` (symbol value).
 *
 * @param str The name of the symbol (string).
 * @param type The type of the symbol.
 * @param other Miscellaneous information about the symbol.
 * @param desc The description field for additional details about the symbol.
 * @param value The value associated with the symbol.
 */
static void put_stabs(const char *str, int type, int other, int desc, unsigned long value)
{
    Stab_Sym *sym;

    sym = section_ptr_add(stab_section, sizeof(Stab_Sym));
    if (str) {
        sym->n_strx = put_elf_str(stabstr_section, str);
    } else {
        sym->n_strx = 0;
    }
    sym->n_type = type;
    sym->n_other = other;
    sym->n_desc = desc;
    sym->n_value = value;
}

/**
 * @brief Add a debugging symbol entry to the STABS section with relocation information.
 *
 * This function adds a debugging symbol entry to the STABS section and also adds a
 * relocation entry to the symbol table section. It creates a new `Stab_Sym` structure
 * based on the provided parameters: `str` (symbol name), `type` (symbol type), `other`
 * (miscellaneous information), `desc` (description), and `value` (symbol value). The
 * function also adds a relocation entry to associate the symbol with the specified
 * `sym_index` in the symbol table section.
 *
 * @param str The name of the symbol (string).
 * @param type The type of the symbol.
 * @param other Miscellaneous information about the symbol.
 * @param desc The description field for additional details about the symbol.
 * @param value The value associated with the symbol.
 * @param sec The section associated with the relocation entry.
 * @param sym_index The index of the symbol in the symbol table section.
 */
static void put_stabs_r(
    const char *str, int type, int other, int desc, unsigned long value, Section *sec, int sym_index)
{
    put_stabs(str, type, other, desc, value);
    put_elf_reloc(symtab_section,
                  stab_section,
                  stab_section->data_offset - sizeof(unsigned int),
                  R_DATA_32,
                  sym_index);
}

/**
 * @brief Add a debugging symbol entry to the STABS section with a numeric value.
 *
 * This function adds a debugging symbol entry to the STABS section with a numeric value.
 * It creates a new `Stab_Sym` structure based on the provided parameters: `type`
 * (symbol type), `other` (miscellaneous information), `desc` (description), and `value`
 * (numeric value). The symbol name is set to `NULL` in this case.
 *
 * @param type The type of the symbol.
 * @param other Miscellaneous information about the symbol.
 * @param desc The description field for additional details about the symbol.
 * @param value The numeric value associated with the symbol.
 */
static void put_stabn(int type, int other, int desc, int value)
{
    put_stabs(NULL, type, other, desc, value);
}

/**
 * @brief Add a debugging symbol entry to the STABS section with no associated value.
 *
 * This function adds a debugging symbol entry to the STABS section with no associated
 * value. It creates a new `Stab_Sym` structure based on the provided parameters: `type`
 * (symbol type), `other` (miscellaneous information), and `desc` (description). The
 * symbol name is set to `NULL` and the value is set to 0 in this case.
 *
 * @param type The type of the symbol.
 * @param other Miscellaneous information about the symbol.
 * @param desc The description field for additional details about the symbol.
 */
static void put_stabd(int type, int other, int desc)
{
    put_stabs(NULL, type, other, desc, 0);
}

#ifndef TCC_TARGET_816
/**
 * @brief Sort symbols in a section and update relocations accordingly.
 *
 * This function sorts symbols in a section and updates the relocations accordingly.
 * It performs two passes: first for local symbols and second for non-local symbols.
 * The function creates new symbol arrays and maps the old symbol indices to the new
 * ones. It then copies the new symbols to the original section data. Finally, it
 * modifies all the relocations associated with the section to reflect the updated
 * symbol indices.
 *
 * @param s1 The TCCState object representing the TCC compiler state.
 * @param s The section containing the symbols to be sorted.
 */
static void sort_syms(TCCState *s1, Section *s)
{
    int *old_to_new_syms;
    ElfW(TokenSym) * new_syms;
    int nb_syms, i;
    ElfW(TokenSym) * p, *q;
    ElfW_Rel *rel, *rel_end;
    Section *sr;
    int type, sym_index;

    nb_syms = s->data_offset / sizeof(ElfW(TokenSym));
    new_syms = tcc_malloc(nb_syms * sizeof(ElfW(TokenSym)));
    old_to_new_syms = tcc_malloc(nb_syms * sizeof(int));

    /* first pass for local symbols */
    p = (ElfW(TokenSym) *) s->data;
    q = new_syms;
    for (i = 0; i < nb_syms; i++) {
        if (ELFW(ST_BIND)(p->st_info) == STB_LOCAL) {
            old_to_new_syms[i] = q - new_syms;
            *q++ = *p;
        }
        p++;
    }
    /* save the number of local symbols in section header */
    s->sh_info = q - new_syms;

    /* then second pass for non local symbols */
    p = (ElfW(TokenSym) *) s->data;
    for (i = 0; i < nb_syms; i++) {
        if (ELFW(ST_BIND)(p->st_info) != STB_LOCAL) {
            old_to_new_syms[i] = q - new_syms;
            *q++ = *p;
        }
        p++;
    }

    /* we copy the new symbols to the old */
    memcpy(s->data, new_syms, nb_syms * sizeof(ElfW(TokenSym)));
    tcc_free(new_syms);

    /* now we modify all the relocations */
    for (i = 1; i < s1->nb_sections; i++) {
        sr = s1->sections[i];
        if (sr->sh_type == SHT_RELX && sr->link == s) {
            rel_end = (ElfW_Rel *) (sr->data + sr->data_offset);
            for (rel = (ElfW_Rel *) sr->data; rel < rel_end; rel++) {
                sym_index = ELFW(R_SYM)(rel->r_info);
                type = ELFW(R_TYPE)(rel->r_info);
                sym_index = old_to_new_syms[sym_index];
                rel->r_info = ELFW(R_INFO)(sym_index, type);
            }
        }
    }

    tcc_free(old_to_new_syms);
}

/**
 * @brief Relocate common symbols to the BSS section.
 *
 * This function relocates common symbols to the BSS (Block Started by Symbol) section.
 * It iterates over the symbol table section and identifies symbols with section index
 * set to SHN_COMMON. For each common symbol, it aligns the symbol value according to
 * its alignment requirement and updates the section index to point to the BSS section.
 * It also updates the data offset of the BSS section to accommodate the relocated
 * symbols.
 */
static void relocate_common_syms(void)
{
    ElfW(TokenSym) * sym, *sym_end;
    unsigned long offset, align;

    sym_end = (ElfW(TokenSym) *) (symtab_section->data + symtab_section->data_offset);
    for (sym = (ElfW(TokenSym) *) symtab_section->data + 1; sym < sym_end; sym++) {
        if (sym->st_shndx == SHN_COMMON) {
            /* align symbol */
            align = sym->st_value;
            offset = bss_section->data_offset;
            offset = (offset + align - 1) & -align;
            sym->st_value = offset;
            sym->st_shndx = bss_section->sh_num;
            offset += sym->st_size;
            bss_section->data_offset = offset;
        }
    }
}

/**
 * @brief Relocate symbols in the symbol table.
 *
 * This function performs symbol relocation in the symbol table. It iterates over
 * the symbol table section and processes each symbol. If a symbol has a section
 * index set to SHN_UNDEF (indicating an undefined symbol), it either resolves the
 * symbol's address or sets the value to zero depending on the value of `do_resolve`.
 * If dynamic symbols exist, the function checks if the symbol is present in the
 * dynamic symbol table and retrieves its value from there. For symbols with a
 * valid section index, the function adds the section base address to the symbol's
 * value.
 *
 * @param s1 The TCCState object representing the TCC compiler state.
 * @param do_resolve Flag indicating whether symbol resolution should be performed
 *                   for undefined symbols.
 */
static void relocate_syms(TCCState *s1, int do_resolve)
{
    ElfW(TokenSym) * sym, *esym, *sym_end;
    int sym_bind, sh_num, sym_index;
    const char *name;

    sym_end = (ElfW(TokenSym) *) (symtab_section->data + symtab_section->data_offset);
    for (sym = (ElfW(TokenSym) *) symtab_section->data + 1; sym < sym_end; sym++) {
        sh_num = sym->st_shndx;
        if (sh_num == SHN_UNDEF) {
            name = strtab_section->data + sym->st_name;
            if (do_resolve) {
#ifndef _WIN32
                void *addr;
                name = symtab_section->link->data + sym->st_name;
                addr = resolve_sym(s1, name);
                if (addr) {
                    sym->st_value = (uplong) addr;
                    goto found;
                }
#endif
            } else if (s1->dynsym) {
                /* if dynamic symbol exist, then use it */
                sym_index = find_elf_sym(s1->dynsym, name);
                if (sym_index) {
                    esym = &((ElfW(TokenSym) *) s1->dynsym->data)[sym_index];
                    sym->st_value = esym->st_value;
                    goto found;
                }
            }
            /* XXX: _fp_hw seems to be part of the ABI, so we ignore
               it */
            if (!strcmp(name, "_fp_hw"))
                goto found;
            /* only weak symbols are accepted to be undefined. Their
               value is zero */
            sym_bind = ELFW(ST_BIND)(sym->st_info);
            if (sym_bind == STB_WEAK) {
                sym->st_value = 0;
            } else {
                error_noabort("undefined symbol '%s'", name);
            }
        } else if (sh_num < SHN_LORESERVE) {
            /* add section base */
            sym->st_value += s1->sections[sym->st_shndx]->sh_addr;
        }
    found:;
    }
}
#endif

#ifndef TCC_TARGET_PE
#ifdef TCC_TARGET_X86_64
#define JMP_TABLE_ENTRY_SIZE 14
/**
 * @brief Add a jump table entry to the runtime PLT and GOT.
 *
 * This function adds a jump table entry to the runtime Procedure Linkage Table (PLT)
 * and Global Offset Table (GOT). It appends the entry to the runtime PLT and GOT,
 * updates the offset, and initializes the entry with the required instructions.
 *
 * @param s1 The TCCState object representing the TCC compiler state.
 * @param val The value to be stored in the jump table entry.
 * @return The address of the added jump table entry.
 */
static unsigned long add_jmp_table(TCCState *s1, unsigned long val)
{
    char *p = s1->runtime_plt_and_got + s1->runtime_plt_and_got_offset;
    s1->runtime_plt_and_got_offset += JMP_TABLE_ENTRY_SIZE;
    /* jmp *0x0(%rip) */
    p[0] = 0xff;
    p[1] = 0x25;
    *(int *) (p + 2) = 0;
    *(unsigned long *) (p + 6) = val;
    return (unsigned long) p;
}

/**
 * @brief Add a Global Offset Table (GOT) entry to the runtime PLT and GOT.
 *
 * This function adds a Global Offset Table (GOT) entry to the runtime Procedure Linkage Table (PLT)
 * and Global Offset Table (GOT). It appends the entry to the runtime PLT and GOT,
 * updates the offset, and initializes the entry with the given value.
 *
 * @param s1 The TCCState object representing the TCC compiler state.
 * @param val The value to be stored in the GOT entry.
 * @return The address of the added GOT entry.
 */
static unsigned long add_got_table(TCCState *s1, unsigned long val)
{
    unsigned long *p = (unsigned long *) (s1->runtime_plt_and_got + s1->runtime_plt_and_got_offset);
    s1->runtime_plt_and_got_offset += sizeof(void *);
    *p = val;
    return (unsigned long) p;
}
#endif
#endif

/**
 * @brief Relocate a given section (CPU dependent).
 *
 * This function performs relocation for a given section based on the specific
 * CPU architecture. It applies the necessary relocations to adjust symbol
 * addresses and resolve references.
 *
 * @param s1 The TCCState object representing the TCC compiler state.
 * @param s The Section object to be relocated.
 */
static void relocate_section(TCCState *s1, Section *s)
{
    Section *sr;
    ElfW_Rel *rel, *rel_end, *qrel;
    ElfW(TokenSym) * sym;
    int type, sym_index;
    unsigned char *ptr;
    unsigned long val, addr;
#if defined TCC_TARGET_I386 || defined TCC_TARGET_X86_64
    int esym_index;
#endif
#ifdef TCC_TARGET_816
    if (!relocptrs) {
        relocptrs = calloc(0x100000, sizeof(char *));
    }
#endif

    sr = s->reloc;
    rel_end = (ElfW_Rel *) (sr->data + sr->data_offset);
    qrel = (ElfW_Rel *) sr->data;
    for (rel = qrel; rel < rel_end; rel++) {
        ptr = s->data + rel->r_offset;

        sym_index = ELFW(R_SYM)(rel->r_info);
        sym = &((ElfW(TokenSym) *) symtab_section->data)[sym_index];
        val = sym->st_value;
#ifdef TCC_TARGET_X86_64
        /* XXX: not tested */
        val += rel->r_addend;
#endif
        type = ELFW(R_TYPE)(rel->r_info);
        addr = s->sh_addr + rel->r_offset;

        /* CPU specific */
        switch (type) {
#if defined(TCC_TARGET_I386)
        case R_386_32:
            if (s1->output_type == TCC_OUTPUT_DLL) {
                esym_index = s1->symtab_to_dynsym[sym_index];
                qrel->r_offset = rel->r_offset;
                if (esym_index) {
                    qrel->r_info = ELFW(R_INFO)(esym_index, R_386_32);
                    qrel++;
                    break;
                } else {
                    qrel->r_info = ELFW(R_INFO)(0, R_386_RELATIVE);
                    qrel++;
                }
            }
            *(int *) ptr += val;
            break;
        case R_386_PC32:
            if (s1->output_type == TCC_OUTPUT_DLL) {
                /* DLL relocation */
                esym_index = s1->symtab_to_dynsym[sym_index];
                if (esym_index) {
                    qrel->r_offset = rel->r_offset;
                    qrel->r_info = ELFW(R_INFO)(esym_index, R_386_PC32);
                    qrel++;
                    break;
                }
            }
            *(int *) ptr += val - addr;
            break;
        case R_386_PLT32:
            *(int *) ptr += val - addr;
            break;
        case R_386_GLOB_DAT:
        case R_386_JMP_SLOT:
            *(int *) ptr = val;
            break;
        case R_386_GOTPC:
            *(int *) ptr += s1->got->sh_addr - addr;
            break;
        case R_386_GOTOFF:
            *(int *) ptr += val - s1->got->sh_addr;
            break;
        case R_386_GOT32:
            /* we load the got offset */
            *(int *) ptr += s1->got_offsets[sym_index];
            break;
        case R_386_16:
            if (s1->output_format != TCC_OUTPUT_FORMAT_BINARY) {
            output_file:
                error("can only produce 16-bit binary files");
            }
            *(short *) ptr += val;
            break;
        case R_386_PC16:
            if (s1->output_format != TCC_OUTPUT_FORMAT_BINARY)
                goto output_file;
            *(short *) ptr += val - addr;
            break;
#elif defined(TCC_TARGET_ARM)
        case R_ARM_PC24:
        case R_ARM_CALL:
        case R_ARM_JUMP24:
        case R_ARM_PLT32: {
            int x;
            x = (*(int *) ptr) & 0xffffff;
            (*(int *) ptr) &= 0xff000000;
            if (x & 0x800000)
                x -= 0x1000000;
            x *= 4;
            x += val - addr;
            if ((x & 3) != 0 || x >= 0x4000000 || x < -0x4000000)
                error("can't relocate value at %x", addr);
            x >>= 2;
            x &= 0xffffff;
            (*(int *) ptr) |= x;
        } break;
        case R_ARM_PREL31: {
            int x;
            x = (*(int *) ptr) & 0x7fffffff;
            (*(int *) ptr) &= 0x80000000;
            x = (x * 2) / 2;
            x += val - addr;
            if ((x ^ (x >> 1)) & 0x40000000)
                error("can't relocate value at %x", addr);
            (*(int *) ptr) |= x & 0x7fffffff;
        }
        case R_ARM_ABS32:
            *(int *) ptr += val;
            break;
        case R_ARM_BASE_PREL:
            *(int *) ptr += s1->got->sh_addr - addr;
            break;
        case R_ARM_GOTOFF32:
            *(int *) ptr += val - s1->got->sh_addr;
            break;
        case R_ARM_GOT_BREL:
            /* we load the got offset */
            *(int *) ptr += s1->got_offsets[sym_index];
            break;
        case R_ARM_COPY:
            break;
        default:
            fprintf(stderr,
                    "FIXME: handle reloc type %x at %lx [%.8x] to %lx\n",
                    type,
                    addr,
                    (unsigned int) (long) ptr,
                    val);
            break;
#elif defined(TCC_TARGET_C67)
        case R_C60_32:
            *(int *) ptr += val;
            break;
        case R_C60LO16: {
            uint32_t orig;

            /* put the low 16 bits of the absolute address */
            // add to what is already there

            orig = ((*(int *) (ptr)) >> 7) & 0xffff;
            orig |= (((*(int *) (ptr + 4)) >> 7) & 0xffff) << 16;

            // patch both at once - assumes always in pairs Low - High

            *(int *) ptr = (*(int *) ptr & (~(0xffff << 7))) | (((val + orig) & 0xffff) << 7);
            *(int *) (ptr + 4) = (*(int *) (ptr + 4) & (~(0xffff << 7)))
                                 | ((((val + orig) >> 16) & 0xffff) << 7);
        } break;
        case R_C60HI16:
            break;
#elif defined(TCC_TARGET_816)
        case R_DATA_32:
            /* no need to change the value at ptr, we only need the offset, and that's already there */
            if (relocptrs[((unsigned long) ptr) & 0xfffff])
                error("relocptrs collision");
            relocptrs[((unsigned long) ptr) & 0xfffff] = symtab_section->link->data + sym->st_name;
            break;
        default:
            fprintf(stderr,
                    "FIXME: handle reloc type %x at %lx [%.8x] to %lx\n",
                    type,
                    addr,
                    (unsigned int) (long) ptr,
                    val);
            break;
#elif defined(TCC_TARGET_X86_64)
        case R_X86_64_64:
            if (s1->output_type == TCC_OUTPUT_DLL) {
                qrel->r_info = ELFW(R_INFO)(0, R_X86_64_RELATIVE);
                qrel->r_addend = *(long long *) ptr + val;
                qrel++;
            }
            *(long long *) ptr += val;
            break;
        case R_X86_64_32:
        case R_X86_64_32S:
            if (s1->output_type == TCC_OUTPUT_DLL) {
                /* XXX: this logic may depend on TCC's codegen
                   now TCC uses R_X86_64_32 even for a 64bit pointer */
                qrel->r_info = ELFW(R_INFO)(0, R_X86_64_RELATIVE);
                qrel->r_addend = *(int *) ptr + val;
                qrel++;
            }
            *(int *) ptr += val;
            break;
        case R_X86_64_PC32: {
            long long diff;
            if (s1->output_type == TCC_OUTPUT_DLL) {
                /* DLL relocation */
                esym_index = s1->symtab_to_dynsym[sym_index];
                if (esym_index) {
                    qrel->r_offset = rel->r_offset;
                    qrel->r_info = ELFW(R_INFO)(esym_index, R_X86_64_PC32);
                    qrel->r_addend = *(int *) ptr;
                    qrel++;
                    break;
                }
            }
            diff = (long long) val - addr;
            if (diff <= -2147483647 || diff > 2147483647) {
#ifndef TCC_TARGET_PE
                /* XXX: naive support for over 32bit jump */
                if (s1->output_type == TCC_OUTPUT_MEMORY) {
                    val = add_jmp_table(s1, val);
                    diff = val - addr;
                }
#endif
                if (diff <= -2147483647 || diff > 2147483647) {
                    error("internal error: relocation failed");
                }
            }
            *(int *) ptr += diff;
        } break;
        case R_X86_64_PLT32:
            *(int *) ptr += val - addr;
            break;
        case R_X86_64_GLOB_DAT:
        case R_X86_64_JUMP_SLOT:
            *(int *) ptr = val;
            break;
        case R_X86_64_GOTPCREL:
#ifndef TCC_TARGET_PE
            if (s1->output_type == TCC_OUTPUT_MEMORY) {
                val = add_got_table(s1, val - rel->r_addend) + rel->r_addend;
                *(int *) ptr += val - addr;
                break;
            }
#endif
            *(int *) ptr += (s1->got->sh_addr - addr + s1->got_offsets[sym_index] - 4);
            break;
        case R_X86_64_GOTTPOFF:
            *(int *) ptr += val - s1->got->sh_addr;
            break;
        case R_X86_64_GOT32:
            /* we load the got offset */
            *(int *) ptr += s1->got_offsets[sym_index];
            break;
#else
#error unsupported processor
#endif
        }
    }
    /* if the relocation is allocated, we change its symbol table */
    if (sr->sh_flags & SHF_ALLOC)
        sr->link = s1->dynsym;
}

#ifndef TCC_TARGET_816
/**
 * @brief Relocate the relocation table in a given section.
 *
 * This function performs relocation of the relocation table in a given section.
 * It adjusts the r_offset field of each relocation entry by adding the sh_addr
 * of the associated section to the original value.
 *
 * @param s1 The TCCState object representing the TCC compiler state.
 * @param sr The Section object representing the relocation table section.
 */
static void relocate_rel(TCCState *s1, Section *sr)
{
    Section *s;
    ElfW_Rel *rel, *rel_end;

    s = s1->sections[sr->sh_info];
    rel_end = (ElfW_Rel *) (sr->data + sr->data_offset);
    for (rel = (ElfW_Rel *) sr->data; rel < rel_end; rel++) {
        rel->r_offset += s->sh_addr;
    }
}

/**
 * @brief Count the number of dynamic relocations in a relocation table section.
 *
 * This function counts the number of dynamic relocations in a given relocation table section.
 * It iterates over each relocation entry and examines the type field to determine if it
 * represents a dynamic relocation. The count of dynamic relocations is returned.
 *
 * @param s1 The TCCState object representing the TCC compiler state.
 * @param sr The Section object representing the relocation table section.
 * @return The number of dynamic relocations in the section.
 */
static int prepare_dynamic_rel(TCCState *s1, Section *sr)
{
    ElfW_Rel *rel, *rel_end;
    int sym_index, esym_index, type, count;

    count = 0;
    rel_end = (ElfW_Rel *) (sr->data + sr->data_offset);
    for (rel = (ElfW_Rel *) sr->data; rel < rel_end; rel++) {
        sym_index = ELFW(R_SYM)(rel->r_info);
        type = ELFW(R_TYPE)(rel->r_info);
        switch (type) {
#if defined(TCC_TARGET_I386)
        case R_386_32:
#elif defined(TCC_TARGET_X86_64)
        case R_X86_64_32:
        case R_X86_64_32S:
        case R_X86_64_64:
#endif
            count++;
            break;
#if defined(TCC_TARGET_I386)
        case R_386_PC32:
#elif defined(TCC_TARGET_X86_64)
        case R_X86_64_PC32:
#endif
            esym_index = s1->symtab_to_dynsym[sym_index];
            if (esym_index)
                count++;
            break;
        default:
            break;
        }
    }
    if (count) {
        /* allocate the section */
        sr->sh_flags |= SHF_ALLOC;
        sr->sh_size = count * sizeof(ElfW_Rel);
    }
    return count;
}

/**
 * @brief Set the Global Offset Table (GOT) offset value.
 *
 * This function sets the value of a specific index in the Global Offset Table (GOT).
 * It takes an index and a value, and stores the value in the corresponding position
 * in the GOT offsets array.
 *
 * @param s1 The TCCState object representing the TCC compiler state.
 * @param index The index of the GOT offset.
 * @param val The value to be stored in the GOT offset.
 */
static void put_got_offset(TCCState *s1, int index, unsigned long val)
{
    int n;
    unsigned long *tab;

    if (index >= s1->nb_got_offsets) {
        /* find immediately bigger power of 2 and reallocate array */
        n = 1;
        while (index >= n)
            n *= 2;
        tab = tcc_realloc(s1->got_offsets, n * sizeof(unsigned long));
        s1->got_offsets = tab;
        memset(s1->got_offsets + s1->nb_got_offsets,
               0,
               (n - s1->nb_got_offsets) * sizeof(unsigned long));
        s1->nb_got_offsets = n;
    }
    s1->got_offsets[index] = val;
}

/**
 * @brief Write a 32-bit value to memory in little-endian format.
 *
 * This function writes a 32-bit value to a memory location specified by the given pointer.
 * The value is written in little-endian format, with the least significant byte placed
 * at the lowest memory address.
 *
 * @param p A pointer to the memory location where the value will be written.
 * @param val The 32-bit value to be written.
 */
static void put32(unsigned char *p, uint32_t val)
{
    p[0] = val;
    p[1] = val >> 8;
    p[2] = val >> 16;
    p[3] = val >> 24;
}

#if defined(TCC_TARGET_I386) || defined(TCC_TARGET_ARM) || defined(TCC_TARGET_X86_64)
/**
 * @brief Read a 32-bit value from memory in little-endian format.
 *
 * This function reads a 32-bit value from a memory location specified by the given pointer.
 * The value is assumed to be stored in little-endian format, with the least significant byte
 * located at the lowest memory address.
 *
 * @param p A pointer to the memory location from which the value will be read.
 * @return The 32-bit value read from memory.
 */
static uint32_t get32(unsigned char *p)
{
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}
#endif

/**
 * @brief Build the Global Offset Table (GOT) section.
 *
 * This function creates the Global Offset Table (GOT) section if it doesn't already exist.
 * The GOT is a data structure used for storing global symbol addresses in position-independent code.
 * It is used for resolving global symbols at runtime.
 *
 * @param s1 The TCC state structure.
 */
static void build_got(TCCState *s1)
{
    unsigned char *ptr;

    /* if no got, then create it */
    s1->got = new_section(s1, ".got", SHT_PROGBITS, SHF_ALLOC | SHF_WRITE);
    s1->got->sh_entsize = 4;
    add_elf_sym(symtab_section,
                0,
                4,
                ELFW(ST_INFO)(STB_GLOBAL, STT_OBJECT),
                0,
                s1->got->sh_num,
                "_GLOBAL_OFFSET_TABLE_");
    ptr = section_ptr_add(s1->got, 3 * PTR_SIZE);
#if PTR_SIZE == 4
    /* keep space for _DYNAMIC pointer, if present */
    put32(ptr, 0);
    /* two dummy got entries */
    put32(ptr + 4, 0);
    put32(ptr + 8, 0);
#else
    /* keep space for _DYNAMIC pointer, if present */
    put32(ptr, 0);
    put32(ptr + 4, 0);
    /* two dummy got entries */
    put32(ptr + 8, 0);
    put32(ptr + 12, 0);
    put32(ptr + 16, 0);
    put32(ptr + 20, 0);
#endif
}

/**
 * @brief Put a Global Offset Table (GOT) entry corresponding to a symbol in `symtab_section`.
 *
 * This function adds a GOT entry for the specified symbol in the symbol table section (`symtab_section`).
 * The `reloc_type`, `size`, `info`, and `sym_index` parameters provide additional information about the symbol.
 * If a GOT entry already exists for the symbol, no new entry is added.
 *
 * @param s1          The TCC state structure.
 * @param reloc_type  The relocation type associated with the symbol.
 * @param size        The size of the symbol.
 * @param info        Additional symbol information, can be modifed if more precise info comes from the DLL.
 * @param sym_index   The index of the symbol in the symbol table.
 */
static void put_got_entry(TCCState *s1, int reloc_type, unsigned long size, int info, int sym_index)
{
    int index;
    const char *name;
    ElfW(TokenSym) * sym;
    unsigned long offset;
    int *ptr;

    if (!s1->got)
        build_got(s1);

    /* if a got entry already exists for that symbol, no need to add one */
    if (sym_index < s1->nb_got_offsets && s1->got_offsets[sym_index] != 0)
        return;

    put_got_offset(s1, sym_index, s1->got->data_offset);

    if (s1->dynsym) {
        sym = &((ElfW(TokenSym) *) symtab_section->data)[sym_index];
        name = symtab_section->link->data + sym->st_name;
        offset = sym->st_value;
#if defined(TCC_TARGET_I386) || defined(TCC_TARGET_X86_64)
        if (reloc_type ==
#ifdef TCC_TARGET_X86_64
            R_X86_64_JUMP_SLOT
#else
            R_386_JMP_SLOT
#endif
        ) {
            Section *plt;
            uint8_t *p;
            int modrm;

#if defined(TCC_OUTPUT_DLL_WITH_PLT)
            modrm = 0x25;
#else
            /* if we build a DLL, we add a %ebx offset */
            if (s1->output_type == TCC_OUTPUT_DLL)
                modrm = 0xa3;
            else
                modrm = 0x25;
#endif

            /* add a PLT entry */
            plt = s1->plt;
            if (plt->data_offset == 0) {
                /* first plt entry */
                p = section_ptr_add(plt, 16);
                p[0] = 0xff; /* pushl got + PTR_SIZE */
                p[1] = modrm + 0x10;
                put32(p + 2, PTR_SIZE);
                p[6] = 0xff; /* jmp *(got + PTR_SIZE * 2) */
                p[7] = modrm;
                put32(p + 8, PTR_SIZE * 2);
            }

            p = section_ptr_add(plt, 16);
            p[0] = 0xff; /* jmp *(got + x) */
            p[1] = modrm;
            put32(p + 2, s1->got->data_offset);
            p[6] = 0x68; /* push $xxx */
            put32(p + 7, (plt->data_offset - 32) >> 1);
            p[11] = 0xe9; /* jmp plt_start */
            put32(p + 12, -(plt->data_offset));

            /* the symbol is modified so that it will be relocated to
               the PLT */
#if !defined(TCC_OUTPUT_DLL_WITH_PLT)
            if (s1->output_type == TCC_OUTPUT_EXE)
#endif
                offset = plt->data_offset - 16;
        }
#elif defined(TCC_TARGET_ARM)
        if (reloc_type == R_ARM_JUMP_SLOT) {
            Section *plt;
            uint8_t *p;

            /* if we build a DLL, we add a %ebx offset */
            if (s1->output_type == TCC_OUTPUT_DLL)
                error("DLLs unimplemented!");

            /* add a PLT entry */
            plt = s1->plt;
            if (plt->data_offset == 0) {
                /* first plt entry */
                p = section_ptr_add(plt, 16);
                put32(p, 0xe52de004);
                put32(p + 4, 0xe59fe010);
                put32(p + 8, 0xe08fe00e);
                put32(p + 12, 0xe5bef008);
            }

            p = section_ptr_add(plt, 16);
            put32(p, 0xe59fc004);
            put32(p + 4, 0xe08fc00c);
            put32(p + 8, 0xe59cf000);
            put32(p + 12, s1->got->data_offset);

            /* the symbol is modified so that it will be relocated to
               the PLT */
            if (s1->output_type == TCC_OUTPUT_EXE)
                offset = plt->data_offset - 16;
        }
#elif defined(TCC_TARGET_C67)
        error("C67 got not implemented");
#else
#error unsupported CPU
#endif
        index = put_elf_sym(s1->dynsym, offset, size, info, 0, sym->st_shndx, name);
        /* put a got entry */
        put_elf_reloc(s1->dynsym, s1->got, s1->got->data_offset, reloc_type, index);
    }
    ptr = section_ptr_add(s1->got, PTR_SIZE);
    *ptr = 0;
}

/**
 * @brief Build Global Offset Table (GOT) and Procedure Linkage Table (PLT) entries.
 *
 * This function iterates over the relocation sections and handles the creation of GOT and PLT entries
 * based on the relocation types. It checks the relocation type and calls `put_got_entry` to add the
 * corresponding GOT entry if necessary.
 *
 * @param s1  The TCC state structure.
 */
static void build_got_entries(TCCState *s1)
{
    Section *s;
    ElfW_Rel *rel, *rel_end;
    ElfW(TokenSym) * sym;
    int i, type, reloc_type, sym_index;

    for (i = 1; i < s1->nb_sections; i++) {
        s = s1->sections[i];
        if (s->sh_type != SHT_RELX)
            continue;
        /* no need to handle got relocations */
        if (s->link != symtab_section)
            continue;
        symtab = s->link;
        rel_end = (ElfW_Rel *) (s->data + s->data_offset);
        for (rel = (ElfW_Rel *) s->data; rel < rel_end; rel++) {
            type = ELFW(R_TYPE)(rel->r_info);
            switch (type) {
#if defined(TCC_TARGET_I386)
            case R_386_GOT32:
            case R_386_GOTOFF:
            case R_386_GOTPC:
            case R_386_PLT32:
                if (!s1->got)
                    build_got(s1);
                if (type == R_386_GOT32 || type == R_386_PLT32) {
                    sym_index = ELFW(R_SYM)(rel->r_info);
                    sym = &((ElfW(TokenSym) *) symtab_section->data)[sym_index];
                    /* look at the symbol got offset. If none, then add one */
                    if (type == R_386_GOT32)
                        reloc_type = R_386_GLOB_DAT;
                    else
                        reloc_type = R_386_JMP_SLOT;
                    put_got_entry(s1, reloc_type, sym->st_size, sym->st_info, sym_index);
                }
                break;
#elif defined(TCC_TARGET_ARM)
            case R_ARM_GOT_BREL:
            case R_ARM_GOTOFF32:
            case R_ARM_BASE_PREL:
            case R_ARM_PLT32:
                if (!s1->got)
                    build_got(s1);
                if (type == R_ARM_GOT_BREL || type == R_ARM_PLT32) {
                    sym_index = ELFW(R_SYM)(rel->r_info);
                    sym = &((ElfW(TokenSym) *) symtab_section->data)[sym_index];
                    /* look at the symbol got offset. If none, then add one */
                    if (type == R_ARM_GOT_BREL)
                        reloc_type = R_ARM_GLOB_DAT;
                    else
                        reloc_type = R_ARM_JUMP_SLOT;
                    put_got_entry(s1, reloc_type, sym->st_size, sym->st_info, sym_index);
                }
                break;
#elif defined(TCC_TARGET_C67)
            case R_C60_GOT32:
            case R_C60_GOTOFF:
            case R_C60_GOTPC:
            case R_C60_PLT32:
                if (!s1->got)
                    build_got(s1);
                if (type == R_C60_GOT32 || type == R_C60_PLT32) {
                    sym_index = ELFW(R_SYM)(rel->r_info);
                    sym = &((ElfW(TokenSym) *) symtab_section->data)[sym_index];
                    /* look at the symbol got offset. If none, then add one */
                    if (type == R_C60_GOT32)
                        reloc_type = R_C60_GLOB_DAT;
                    else
                        reloc_type = R_C60_JMP_SLOT;
                    put_got_entry(s1, reloc_type, sym->st_size, sym->st_info, sym_index);
                }
                break;
#elif defined(TCC_TARGET_X86_64)
            case R_X86_64_GOT32:
            case R_X86_64_GOTTPOFF:
            case R_X86_64_GOTPCREL:
            case R_X86_64_PLT32:
                if (!s1->got)
                    build_got(s1);
                if (type == R_X86_64_GOT32 || type == R_X86_64_GOTPCREL || type == R_X86_64_PLT32) {
                    sym_index = ELFW(R_SYM)(rel->r_info);
                    sym = &((ElfW(TokenSym) *) symtab_section->data)[sym_index];
                    /* look at the symbol got offset. If none, then add one */
                    if (type == R_X86_64_GOT32 || type == R_X86_64_GOTPCREL)
                        reloc_type = R_X86_64_GLOB_DAT;
                    else
                        reloc_type = R_X86_64_JUMP_SLOT;
                    put_got_entry(s1, reloc_type, sym->st_size, sym->st_info, sym_index);
                }
                break;
#else
#error unsupported CPU
#endif
            default:
                break;
            }
        }
    }
}
#endif

/**
 * @brief Create a new symbol table section.
 *
 * This function creates a new symbol table section with the specified attributes. It also creates
 * a corresponding string table section and a hash table section for symbol lookup.
 *
 * @param s1              The TCC state structure.
 * @param symtab_name     The name of the symbol table section.
 * @param sh_type         The type of the symbol table section.
 * @param sh_flags        The flags of the symbol table section.
 * @param strtab_name     The name of the string table section.
 * @param hash_name       The name of the hash table section.
 * @param hash_sh_flags   The flags of the hash table section.
 *
 * @return The created symbol table section.
 */
static Section *new_symtab(TCCState *s1,
                           const char *symtab_name,
                           int sh_type,
                           int sh_flags,
                           const char *strtab_name,
                           const char *hash_name,
                           int hash_sh_flags)
{
    Section *symtab, *strtab, *hash;
    int *ptr, nb_buckets;

    symtab = new_section(s1, symtab_name, sh_type, sh_flags);
    symtab->sh_entsize = sizeof(ElfW(TokenSym));
    strtab = new_section(s1, strtab_name, SHT_STRTAB, sh_flags);
    put_elf_str(strtab, "");
    symtab->link = strtab;
    put_elf_sym(symtab, 0, 0, 0, 0, 0, NULL);

    nb_buckets = 1;

    hash = new_section(s1, hash_name, SHT_HASH, hash_sh_flags);
    hash->sh_entsize = sizeof(int);
    symtab->hash = hash;
    hash->link = symtab;

    ptr = section_ptr_add(hash, (2 + nb_buckets + 1) * sizeof(int));
    ptr[0] = nb_buckets;
    ptr[1] = 1;
    memset(ptr + 2, 0, (nb_buckets + 1) * sizeof(int));
    return symtab;
}

#ifndef TCC_TARGET_816
/**
 * @brief Add a dynamic tag to the dynamic section.
 *
 * This function adds a dynamic tag with the specified tag value and value to the dynamic section.
 *
 * @param dynamic   The dynamic section to add the tag to.
 * @param dt        The tag value.
 * @param val       The value associated with the tag.
 */
static void put_dt(Section *dynamic, int dt, unsigned long val)
{
    ElfW(Dyn) * dyn;
    dyn = section_ptr_add(dynamic, sizeof(ElfW(Dyn)));
    dyn->d_tag = dt;
    dyn->d_un.d_val = val;
}

/**
 * @brief Add the defines for the initialization array start and end symbols.
 *
 * This function adds the start and end symbols for the initialization array of a given section.
 * The symbols are added to the symbol table section.
 *
 * @param s1            The TCC state structure.
 * @param section_name  The name of the section.
 */
static void add_init_array_defines(TCCState *s1, const char *section_name)
{
    Section *s;
    long end_offset;
    char sym_start[1024];
    char sym_end[1024];

    snprintf(sym_start, sizeof(sym_start), "__%s_start", section_name + 1);
    snprintf(sym_end, sizeof(sym_end), "__%s_end", section_name + 1);

    s = find_section(s1, section_name);
    if (!s) {
        end_offset = 0;
        s = data_section;
    } else {
        end_offset = s->data_offset;
    }

    add_elf_sym(symtab_section, 0, 0, ELFW(ST_INFO)(STB_GLOBAL, STT_NOTYPE), 0, s->sh_num, sym_start);
    add_elf_sym(symtab_section,
                end_offset,
                0,
                ELFW(ST_INFO)(STB_GLOBAL, STT_NOTYPE),
                0,
                s->sh_num,
                sym_end);
}

static void tcc_add_bcheck(TCCState *s1)
{
#ifdef CONFIG_TCC_BCHECK
    unsigned long *ptr;
    Section *init_section;
    unsigned char *pinit;
    int sym_index;

    if (0 == s1->do_bounds_check)
        return;

    /* XXX: add an object file to do that */
    ptr = section_ptr_add(bounds_section, sizeof(unsigned long));
    *ptr = 0;
    add_elf_sym(symtab_section,
                0,
                0,
                ELFW(ST_INFO)(STB_GLOBAL, STT_NOTYPE),
                0,
                bounds_section->sh_num,
                "__bounds_start");
    /* add bound check code */
#ifndef TCC_TARGET_PE
    {
        char buf[1024];
        snprintf(buf, sizeof(buf), "%s/%s", s1->tcc_lib_path, "bcheck.o");
        tcc_add_file(s1, buf);
    }
#endif
#ifdef TCC_TARGET_I386
    if (s1->output_type != TCC_OUTPUT_MEMORY) {
        /* add 'call __bound_init()' in .init section */
        init_section = find_section(s1, ".init");
        pinit = section_ptr_add(init_section, 5);
        pinit[0] = 0xe8;
        put32(pinit + 1, -4);
        sym_index = find_elf_sym(symtab_section, "__bound_init");
        put_elf_reloc(symtab_section,
                      init_section,
                      init_section->data_offset - 4,
                      R_386_PC32,
                      sym_index);
    }
#endif
#endif
}

/**
 * @brief Add the TCC runtime libraries.
 *
 * This function adds the TCC runtime libraries to the compilation process.
 * It adds the necessary object files and libraries based on the TCC configuration.
 *
 * @param s1    The TCC state structure.
 */
static void tcc_add_runtime(TCCState *s1)
{
    tcc_add_bcheck(s1);

    /* add libc */
    if (!s1->nostdlib) {
#ifdef CONFIG_USE_LIBGCC
        tcc_add_library(s1, "c");
        tcc_add_file(s1, CONFIG_SYSROOT "/lib/libgcc_s.so.1");
#else
        char buf[1024];
        tcc_add_library(s1, "c");
        snprintf(buf, sizeof(buf), "%s/%s", s1->tcc_lib_path, "libtcc1.a");
        tcc_add_file(s1, buf);
#endif
    }
    /* add crt end if not memory output */
    if (s1->output_type != TCC_OUTPUT_MEMORY && !s1->nostdlib) {
        tcc_add_file(s1, CONFIG_TCC_CRT_PREFIX "/crtn.o");
    }
}

/**
 * @brief Add various standard linker symbols.
 *
 * This function adds standard linker symbols such as _etext, _edata, and _end
 * to the symbol table section. It also adds start and stop symbols for sections
 * whose names can be expressed in C. Additionally, it adds defines for the
 * initialization and finalization arrays (preinit_array, init_array, fini_array)
 * used in ldscripts.
 *
 * @param s1    The TCC state structure.
 */
static void tcc_add_linker_symbols(TCCState *s1)
{
    char buf[1024];
    int i;
    Section *s;

    add_elf_sym(symtab_section,
                text_section->data_offset,
                0,
                ELFW(ST_INFO)(STB_GLOBAL, STT_NOTYPE),
                0,
                text_section->sh_num,
                "_etext");
    add_elf_sym(symtab_section,
                data_section->data_offset,
                0,
                ELFW(ST_INFO)(STB_GLOBAL, STT_NOTYPE),
                0,
                data_section->sh_num,
                "_edata");
    add_elf_sym(symtab_section,
                bss_section->data_offset,
                0,
                ELFW(ST_INFO)(STB_GLOBAL, STT_NOTYPE),
                0,
                bss_section->sh_num,
                "_end");
    /* horrible new standard ldscript defines */
    add_init_array_defines(s1, ".preinit_array");
    add_init_array_defines(s1, ".init_array");
    add_init_array_defines(s1, ".fini_array");

    /* add start and stop symbols for sections whose name can be
       expressed in C */
    for (i = 1; i < s1->nb_sections; i++) {
        s = s1->sections[i];
        if (s->sh_type == SHT_PROGBITS && (s->sh_flags & SHF_ALLOC)) {
            const char *p;
            int ch;

            /* check if section name can be expressed in C */
            p = s->name;
            for (;;) {
                ch = *p;
                if (!ch)
                    break;
                if (!isid(ch) && !isnum(ch))
                    goto next_sec;
                p++;
            }
            snprintf(buf, sizeof(buf), "__start_%s", s->name);
            add_elf_sym(symtab_section,
                        0,
                        0,
                        ELFW(ST_INFO)(STB_GLOBAL, STT_NOTYPE),
                        0,
                        s->sh_num,
                        buf);
            snprintf(buf, sizeof(buf), "__stop_%s", s->name);
            add_elf_sym(symtab_section,
                        s->data_offset,
                        0,
                        ELFW(ST_INFO)(STB_GLOBAL, STT_NOTYPE),
                        0,
                        s->sh_num,
                        buf);
        }
    next_sec:;
    }
}

/* name of ELF interpreter */
#if defined __FreeBSD__
static const char elf_interp[] = "/libexec/ld-elf.so.1";
#elif defined TCC_ARM_EABI
static const char elf_interp[] = "/lib/ld-linux.so.3";
#elif defined(TCC_TARGET_X86_64)
static const char elf_interp[] = "/lib/ld-linux-x86-64.so.2";
#elif defined(TCC_UCLIBC)
static const char elf_interp[] = "/lib/ld-uClibc.so.0";
#else
static const char elf_interp[] = "/lib/ld-linux.so.2";
#endif
#endif

/**
 * @brief Output the binary executable file.
 *
 * This function writes the binary executable file to the specified file stream.
 * The output format is determined by the target architecture and can be an
 * object file, an executable file, or any other binary format supported by TCC.
 *
 * @param s1              The TCC state structure.
 * @param f               The file stream to write the output to.
 * @param section_order   An array specifying the order in which sections should
 *                        be written to the output. It is an optional parameter,
 *                        and if NULL, the default order is used.
 */
static void tcc_output_binary(TCCState *s1, FILE *f, const int *section_order)
{
#ifndef TCC_TARGET_816
    Section *s;
    int i, offset, size;

    offset = 0;
    for (i = 1; i < s1->nb_sections; i++) {
        s = s1->sections[section_order[i]];
        if (s->sh_type != SHT_NOBITS && (s->sh_flags & SHF_ALLOC)) {
            while (offset < s->sh_offset) {
                fputc(0, f);
                offset++;
            }
            size = s->sh_size;
            fwrite(s->data, 1, size, f);
            offset += size;
        }
    }
#else
    Section *s;
    int i, j, k, size;

    /* include header */
    fprintf(f, ".include \"hdr.asm\"\n");
    fprintf(f, ".accu 16\n.index 16\n");
    fprintf(f, ".16bit\n");
    if (s1->hirom_comp) {
        if (s1->fastrom_comp)
            fprintf(f, ".BASE $C0\n"); /* HiRom - FastROM */
        else
            fprintf(f, ".BASE $40\n"); /* HiRom - slowROM */
    } else if (s1->fastrom_comp)
        fprintf(f, ".BASE $80\n"); /* LoRom - FastROM */

    /* local variable size constants; used to be generated as part of the
       function epilog, but WLA DX barfed once in a while about missing
       symbols. putting them at the start of the file works around that. */
    for (i = 0; i < localno; i++) {
        fprintf(f, ".define __%s_locals %d\n", locals[i], localnos[i]);
    }

    /* relocate sections
       this not only rewrites the pointers inside sections (with bogus
       data), but, more importantly, saves the names of the symbols we have
       to output later in place of this bogus data in the relocptrs[] array. */
    for (i = 1; i < s1->nb_sections; i++) {
        s = s1->sections[section_order[i]];
        if (s->reloc && s != s1->got)
            relocate_section(s1, s);
    }

    /* output sections */
    for (i = 1; i < s1->nb_sections; i++) {
        s = s1->sections[section_order[i]];

        /* these sections are meaningless when writing plain-text assembler output */
        if (strcmp(".symtab", s->name) == 0 || strcmp(".strtab", s->name) == 0
            || strcmp(".rel.data", s->name) == 0 || strcmp(".shstrtab", s->name) == 0)
            continue;

        size = s->sh_size; /* section size in bytes */

        if (s == text_section) {
            /* functions each have their own section (otherwise WLA DX is
               not able to allocate ROM space for them efficiently), so we
               do not have to print a function header here */
            int next_jump_pos
                = 0; /* the next offset in the text section where we will look for a jump target */
            for (j = 0; j < size; j++) {
                for (k = 0; k < labels; k++) {
                    if (label[k].pos == j)
                        fprintf(f, "%s%s:\n", STATIC_PREFIX /* "__local_" */, label[k].name);
                }
                /* insert jump labels */
                if (next_jump_pos == j) {
                    next_jump_pos = size;
                    for (k = 0; k < jumps; k++) {
                        /* while we're here, look for the next jump target after this one */
                        if (jump[k][1] > j && jump[k][1] < next_jump_pos)
                            next_jump_pos = jump[k][1];
                        /* write the jump target label(s) for this position */
                        if (jump[k][1] == j)
                            fprintf(f, LOCAL_LABEL ":\n", k);
                    }
                }
                fputc(s->data[j], f);
            }
            if (!section_closed)
                fprintf(f, ".ENDS\n");
        } else if (s == bss_section) {
            /* uninitialized data, we only need a .ramsection */
            ElfW(TokenSym) * esym;
            int empty = 1;
            if (s1->hirom_comp || s1->fastrom_comp)
                fprintf(f, ".BASE $00\n"); /* Return to base $00 */
            fprintf(f, ".RAMSECTION \".bss\" BANK $7e SLOT 2\n");
            for (j = 0, esym = (ElfW(TokenSym) *) symtab_section->data;
                 j < symtab_section->sh_size / sizeof(ElfW(TokenSym));
                 esym++, j++) {
                if (esym->st_shndx == SHN_COMMON
                    && strlen(symtab_section->link->data
                              + esym->st_name)) /* omit nameless symbols (fixes 20041218-1.c) */
                {
                    /* looks like these are the symbols that need to go here,
                       but that is merely an educated guess. works for me, though. */
                    fprintf(f,
                            "%s%s dsb %d\n",
                            /*ELF32_ST_BIND(esym->st_info) == STB_LOCAL ? STATIC_PREFIX:*/ "",
                            symtab_section->link->data + esym->st_name,
                            esym->st_size);
                    empty = 0;
                }
            }

            fprintf(f, ".ENDS\n");
        } else { /* .data, .rodata, user-defined sections */

            int deebeed = 0; /* remembers whether we have printed ".DB"
                                  before; reset after a newline or a
                                  different sized prefix, e.g. ".DW" */
            int startk = 0;  /* 0 == .ramsection, 1 == .section */
            int endk = 2;    /* do both by default */

            if (s != data_section)
                startk = 1; /* only do .section (.rodata and user sections go to ROM) */

            int bytecount = 0; /* how many bytes to reserve in .ramsection */

            /* k == 0: output .ramsection; k == 1: output .section */
            for (k = startk; k < endk; k++) {
                if (k == 0) { /* .ramsection */
                    if (s1->hirom_comp || s1->fastrom_comp)
                        fprintf(f, ".BASE $00\n"); /* Return to base $00 */
                    fprintf(f,
                            ".RAMSECTION \"ram%s%s\" APPENDTO \"globram.data\"\n",
                            unique_token,
                            s->name);
                } else { /* (ROM) .section */
                    // check for .data section to append to global one
                    if (!strcmp(s->name, ".data"))
                        fprintf(f,
                                ".SECTION \"%s%s\" APPENDTO \"glob.data\"\n",
                                unique_token,
                                s->name);
                    else {
                        if (s1->hirom_comp)
                            fprintf(f, ".SECTION \"%s\" SEMIFREE ORG $8000\n",
                                    s->name); // 09042021
                        else
                            fprintf(f, ".SECTION \"%s\" SUPERFREE\n",
                                    s->name); // 09042021
                    }
                }

                // int next_symbol_pos = 0; /* position inside the section at which to look for the next symbol */
                for (j = 0; j < size; j++) {
                    // TokenSym* ps = global_stack;
                    int ps;

                    /* check if there is a symbol at this position */
                    ElfW(TokenSym) * esym; /* ELF symbol */
                    char *lastsym
                        = NULL; /* name of previous symbol (some symbols appear more than once; bug?) */
                    int symbol_printed = 0; /* have we already printed a symbol in this run? */
                    for (ps = 0, esym = (ElfW(TokenSym) *) symtab_section->data;
                         ps < symtab_section->sh_size / sizeof(ElfW(TokenSym));
                         esym++, ps++) {
                        // if(!find_elf_sym(symtab_section, get_tok_str(ps->v, NULL))) continue;
                        unsigned long pval;
                        char *symname = symtab_section->link->data + esym->st_name;
                        char *symprefix = "";

                        /* we do not have to care about external references (handled by the linker) or
                           functions (handled by the code generator */
                        // if((ps->type.t & VT_EXTERN) || ((ps->type.t & VT_BTYPE) == VT_FUNC)) continue;

                        /* look up this symbol */
                        pval = esym->st_value;

                        /* look for the next symbol after this one */
                        // if(pval > j && pval < next_symbol_pos) next_symbol_pos = pval;

                        /* Is this symbol at this position and in this section? */
                        if (pval != j || esym->st_shndx != s->sh_num)
                            continue;

                        /* skip empty symbols (bug?) */
                        if (strlen(symname) == 0)
                            continue;
                        /* some symbols appear more than once; avoid defining them more than once (bug?) */
                        if (lastsym && !strcmp(lastsym, symname))
                            continue;
                        /* remember this symbol for the next iteration */
                        lastsym = symname;

                        /* if this is a local (static) symbol, prefix it so the assembler knows this
                            is file-local. */
                        /* FIXME: breaks for local static variables (name collision) */
                        // if(ELF32_ST_BIND(esym->st_info) == STB_LOCAL) { symprefix = STATIC_PREFIX; }

                        /* if this is a ramsection, we now know how large the _previous_ symbol was; print it. */
                        /* if we already printed a symbol in this section, define this symbol as size 0 so it
                            gets the same address as the other ones at this position. */
                        if (k == 0 && (bytecount > 0 || symbol_printed)) {
                            fprintf(f, "dsb %d", bytecount);
                            bytecount = 0;
                        }

                        /* if there are two sections, print label only in .ramsection */
                        if (k == 0)
                            fprintf(f, "\n%s%s ", symprefix, symname);
                        else if (startk == 1)
                            fprintf(f, "\n%s%s: ", symprefix, symname);
                        else
                            fprintf(f, "\n");
                        symbol_printed = 1;
                    }

                    if (symbol_printed) {
                        /* pointers and arrays may have a symbolic name. find out if that's the case.
                             everything else is literal and handled later */
                        unsigned int ptr = *((unsigned int *) &s->data[j]);
                        unsigned char ptrc = *((unsigned char *) &s->data[j]);

                        if (k == 0) { /* .ramsection, just count bytes */
                            bytecount++;
                        } else { /* (ROM) .section, need to output data */
                            if (relocptrs && relocptrs[((unsigned long) &s->data[j]) & 0xfffff]) {
                                /* relocated -> print a symbolic pointer */
                                char *ptrname = relocptrs[((unsigned long) &s->data[j]) & 0xfffff];
                                fprintf(f, ".dw %s + %d, :%s", ptrname, ptr, ptrname);
                                j += 3; /* we have handled 3 more bytes than expected */
                                deebeed = 0;
                            } else {
                                /* any non-symbolic data; print one byte, then let the generic code take over */
                                fprintf(f, ".db $%x", ptrc);
                                deebeed = 1;
                            }
                        }
                        continue; /* data has been printed, go ahead */
                    }

                    /* no symbol here, just print the data */
                    if (k == 1 && relocptrs && relocptrs[((unsigned long) &s->data[j]) & 0xfffff]) {
                        /* unlabeled data may have been relocated, too */
                        fprintf(f,
                                "\n.dw %s + %d\n.dw :%s",
                                relocptrs[((unsigned long) &s->data[j]) & 0xfffff],
                                *(unsigned int *) (&s->data[j]),
                                relocptrs[((unsigned long) &s->data[j]) & 0xfffff]);
                        j += 3;
                        deebeed = 0;
                        continue;
                    }

                    if (!deebeed) {
                        if (k == 1)
                            fprintf(f, "\n.db ");
                        deebeed = 1;
                    } else if (k == 1)
                        fprintf(f, ",");
                    if (k == 1)
                        fprintf(f, "$%x", s->data[j]);
                    bytecount++;
                }

                if (k == 0) {
                    if (bytecount) // 07/06/2023, added because of previous test removed
                        fprintf(f, "dsb %d\n", bytecount);
                    bytecount = 0;
                }
                fprintf(f, "\n.ENDS\n\n");
            }
        }
    }
#endif
}

#if defined(__FreeBSD__)
#define HAVE_PHDR 1
#define EXTRA_RELITEMS 14

/* move the relocation value from .dynsym to .got */
void patch_dynsym_undef(TCCState *s1, Section *s)
{
    uint32_t *gotd = (void *) s1->got->data;
    ElfW(TokenSym) * sym, *sym_end;

    gotd += 3; // dummy entries in .got
    /* relocate symbols in .dynsym */
    sym_end = (ElfW(TokenSym) *) (s->data + s->data_offset);
    for (sym = (ElfW(TokenSym) *) s->data + 1; sym < sym_end; sym++) {
        if (sym->st_shndx == SHN_UNDEF) {
            *gotd++ = sym->st_value + 6; // XXX 6 is magic ?
            sym->st_value = 0;
        }
    }
}
#else
#define HAVE_PHDR 0
#define EXTRA_RELITEMS 9
#endif

/**
 * @brief Output an ELF file.
 *
 * This function writes the ELF file to the specified filename. The ELF file
 * contains the compiled code, symbols, sections, and other relevant information
 * based on the TCCState structure.
 *
 * @param s1        The TCC state structure.
 * @param filename  The name of the output ELF file.
 * @return          0 on success, -1 on failure.
 */
int elf_output_file(TCCState *s1, const char *filename)
{
#ifndef TCC_TARGET_816
    ElfW(Ehdr) ehdr;
    FILE *f;
    int fd, mode, ret;
    int *section_order;
    int shnum, i, phnum, file_offset, offset, size, j, tmp, sh_order_index, k;
    unsigned long addr;
    Section *strsec, *s;
    ElfW(Shdr) shdr, *sh;
    ElfW(Phdr) * phdr, *ph;
    Section *interp, *dynamic, *dynstr;
    unsigned long saved_dynamic_data_offset;
    ElfW(TokenSym) * sym;
    int type, file_type;
    unsigned long rel_addr, rel_size;
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
    unsigned long bss_addr, bss_size;
#endif

    file_type = s1->output_type;
    s1->nb_errors = 0;

    if (file_type != TCC_OUTPUT_OBJ) {
        tcc_add_runtime(s1);
    }

    phdr = NULL;
    section_order = NULL;
    interp = NULL;
    dynamic = NULL;
    dynstr = NULL;                 /* avoid warning */
    saved_dynamic_data_offset = 0; /* avoid warning */

    if (file_type != TCC_OUTPUT_OBJ) {
        relocate_common_syms();

        tcc_add_linker_symbols(s1);

        if (!s1->static_link) {
            const char *name;
            int sym_index, index;
            ElfW(TokenSym) * esym, *sym_end;

            if (file_type == TCC_OUTPUT_EXE) {
                char *ptr;
                /* allow override the dynamic loader */
                const char *elfint = getenv("LD_SO");
                if (elfint == NULL)
                    elfint = elf_interp;
                /* add interpreter section only if executable */
                interp = new_section(s1, ".interp", SHT_PROGBITS, SHF_ALLOC);
                interp->sh_addralign = 1;
                ptr = section_ptr_add(interp, 1 + strlen(elfint));
                strcpy(ptr, elfint);
            }

            /* add dynamic symbol table */
            s1->dynsym
                = new_symtab(s1, ".dynsym", SHT_DYNSYM, SHF_ALLOC, ".dynstr", ".hash", SHF_ALLOC);
            dynstr = s1->dynsym->link;

            /* add dynamic section */
            dynamic = new_section(s1, ".dynamic", SHT_DYNAMIC, SHF_ALLOC | SHF_WRITE);
            dynamic->link = dynstr;
            dynamic->sh_entsize = sizeof(ElfW(Dyn));

            /* add PLT */
            s1->plt = new_section(s1, ".plt", SHT_PROGBITS, SHF_ALLOC | SHF_EXECINSTR);
            s1->plt->sh_entsize = 4;

            build_got(s1);

            /* scan for undefined symbols and see if they are in the
               dynamic symbols. If a symbol STT_FUNC is found, then we
               add it in the PLT. If a symbol STT_OBJECT is found, we
               add it in the .bss section with a suitable relocation */
            sym_end = (ElfW(TokenSym) *) (symtab_section->data + symtab_section->data_offset);
            if (file_type == TCC_OUTPUT_EXE) {
                for (sym = (ElfW(TokenSym) *) symtab_section->data + 1; sym < sym_end; sym++) {
                    if (sym->st_shndx == SHN_UNDEF) {
                        name = symtab_section->link->data + sym->st_name;
                        sym_index = find_elf_sym(s1->dynsymtab_section, name);
                        if (sym_index) {
                            esym = &((ElfW(TokenSym) *) s1->dynsymtab_section->data)[sym_index];
                            type = ELFW(ST_TYPE)(esym->st_info);
                            if (type == STT_FUNC) {
                                put_got_entry(s1,
                                              R_JMP_SLOT,
                                              esym->st_size,
                                              esym->st_info,
                                              sym - (ElfW(TokenSym) *) symtab_section->data);
                            } else if (type == STT_OBJECT) {
                                unsigned long offset;
                                offset = bss_section->data_offset;
                                /* XXX: which alignment ? */
                                offset = (offset + 16 - 1) & -16;
                                index = put_elf_sym(s1->dynsym,
                                                    offset,
                                                    esym->st_size,
                                                    esym->st_info,
                                                    0,
                                                    bss_section->sh_num,
                                                    name);
                                put_elf_reloc(s1->dynsym, bss_section, offset, R_COPY, index);
                                offset += esym->st_size;
                                bss_section->data_offset = offset;
                            }
                        } else {
                            /* STB_WEAK undefined symbols are accepted */
                            /* XXX: _fp_hw seems to be part of the ABI, so we ignore
                               it */
                            if (ELFW(ST_BIND)(sym->st_info) == STB_WEAK || !strcmp(name, "_fp_hw")) {
                            } else {
                                error_noabort("undefined symbol '%s'", name);
                            }
                        }
                    } else if (s1->rdynamic && ELFW(ST_BIND)(sym->st_info) != STB_LOCAL) {
                        /* if -rdynamic option, then export all non
                           local symbols */
                        name = symtab_section->link->data + sym->st_name;
                        put_elf_sym(s1->dynsym,
                                    sym->st_value,
                                    sym->st_size,
                                    sym->st_info,
                                    0,
                                    sym->st_shndx,
                                    name);
                    }
                }

                if (s1->nb_errors)
                    goto fail;

                /* now look at unresolved dynamic symbols and export
                   corresponding symbol */
                sym_end = (ElfW(TokenSym) *) (s1->dynsymtab_section->data
                                         + s1->dynsymtab_section->data_offset);
                for (esym = (ElfW(TokenSym) *) s1->dynsymtab_section->data + 1; esym < sym_end; esym++) {
                    if (esym->st_shndx == SHN_UNDEF) {
                        name = s1->dynsymtab_section->link->data + esym->st_name;
                        sym_index = find_elf_sym(symtab_section, name);
                        if (sym_index) {
                            /* XXX: avoid adding a symbol if already
                               present because of -rdynamic ? */
                            sym = &((ElfW(TokenSym) *) symtab_section->data)[sym_index];
                            put_elf_sym(s1->dynsym,
                                        sym->st_value,
                                        sym->st_size,
                                        sym->st_info,
                                        0,
                                        sym->st_shndx,
                                        name);
                        } else {
                            if (ELFW(ST_BIND)(esym->st_info) == STB_WEAK) {
                                /* weak symbols can stay undefined */
                            } else {
                                warning("undefined dynamic symbol '%s'", name);
                            }
                        }
                    }
                }
            } else {
                int nb_syms;
                /* shared library case : we simply export all the global symbols */
                nb_syms = symtab_section->data_offset / sizeof(ElfW(TokenSym));
                s1->symtab_to_dynsym = tcc_mallocz(sizeof(int) * nb_syms);
                for (sym = (ElfW(TokenSym) *) symtab_section->data + 1; sym < sym_end; sym++) {
                    if (ELFW(ST_BIND)(sym->st_info) != STB_LOCAL) {
#if defined(TCC_OUTPUT_DLL_WITH_PLT)
                        if (ELFW(ST_TYPE)(sym->st_info) == STT_FUNC && sym->st_shndx == SHN_UNDEF) {
                            put_got_entry(s1,
                                          R_JMP_SLOT,
                                          sym->st_size,
                                          sym->st_info,
                                          sym - (ElfW(TokenSym) *) symtab_section->data);
                        } else if (ELFW(ST_TYPE)(sym->st_info) == STT_OBJECT) {
                            put_got_entry(s1,
                                          R_X86_64_GLOB_DAT,
                                          sym->st_size,
                                          sym->st_info,
                                          sym - (ElfW(TokenSym) *) symtab_section->data);
                        } else
#endif
                        {
                            name = symtab_section->link->data + sym->st_name;
                            index = put_elf_sym(s1->dynsym,
                                                sym->st_value,
                                                sym->st_size,
                                                sym->st_info,
                                                0,
                                                sym->st_shndx,
                                                name);
                            s1->symtab_to_dynsym[sym - (ElfW(TokenSym) *) symtab_section->data] = index;
                        }
                    }
                }
            }

            build_got_entries(s1);

            /* add a list of needed dlls */
            for (i = 0; i < s1->nb_loaded_dlls; i++) {
                DLLReference *dllref = s1->loaded_dlls[i];
                if (dllref->level == 0)
                    put_dt(dynamic, DT_NEEDED, put_elf_str(dynstr, dllref->name));
            }

            if (s1->rpath)
                put_dt(dynamic, DT_RPATH, put_elf_str(dynstr, s1->rpath));

            /* XXX: currently, since we do not handle PIC code, we
               must relocate the readonly segments */
            if (file_type == TCC_OUTPUT_DLL) {
                if (s1->soname)
                    put_dt(dynamic, DT_SONAME, put_elf_str(dynstr, s1->soname));
                put_dt(dynamic, DT_TEXTREL, 0);
            }
            /* add necessary space for other entries */
            saved_dynamic_data_offset = dynamic->data_offset;
            dynamic->data_offset += sizeof(ElfW(Dyn)) * EXTRA_RELITEMS;
        } else {
            /* still need to build got entries in case of static link */
            build_got_entries(s1);
        }
    }
#else
    ElfW(Ehdr) ehdr;
    FILE *f;
    int fd, mode, ret;
    int *section_order;
    int shnum, i, phnum, file_offset, sh_order_index;
    Section *strsec, *s;
    ElfW(Phdr) * phdr;
    Section *interp, *dynamic, *dynstr;
    unsigned long saved_dynamic_data_offset;
    int file_type;

    file_type = s1->output_type;
    s1->nb_errors = 0;

    if (file_type != TCC_OUTPUT_OBJ) {
        abort();
    }

    phdr = NULL;
    section_order = NULL;
    interp = NULL;
    dynamic = NULL;
    /* avoid warning */
    dynstr = NULL;
    /* avoid warning */
    saved_dynamic_data_offset = 0;

    if (file_type != TCC_OUTPUT_OBJ) {
        abort();
    }
#endif

    memset(&ehdr, 0, sizeof(ehdr));

    /* we add a section for symbols */
    strsec = new_section(s1, ".shstrtab", SHT_STRTAB, 0);
    put_elf_str(strsec, "");

    /* compute number of sections */
    shnum = s1->nb_sections;

    /* this array is used to reorder sections in the output file */
    section_order = tcc_malloc(sizeof(int) * shnum);
    section_order[0] = 0;
    sh_order_index = 1;

    /* compute number of program headers */
    switch (file_type) {
    default:
    case TCC_OUTPUT_OBJ:
        phnum = 0;
        break;
    case TCC_OUTPUT_EXE:
        if (!s1->static_link)
            phnum = 4 + HAVE_PHDR;
        else
            phnum = 2;
        break;
    case TCC_OUTPUT_DLL:
        phnum = 3;
        break;
    }

    /* allocate strings for section names and decide if an unallocated
       section should be output */
    /* NOTE: the strsec section comes last, so its size is also
       correct ! */
    for (i = 1; i < s1->nb_sections; i++) {
        s = s1->sections[i];
        s->sh_name = put_elf_str(strsec, s->name);
#if 0 // gr
        printf("section: f=%08x t=%08x i=%08x %s %s\n",
               s->sh_flags,
               s->sh_type,
               s->sh_info,
               s->name,
               s->reloc ? s->reloc->name : "n"
               );
#endif
        /* when generating a DLL, we include relocations but we may
           patch them */
        if (file_type == TCC_OUTPUT_DLL && s->sh_type == SHT_RELX && !(s->sh_flags & SHF_ALLOC)) {
#ifdef TCC_TARGET_816
            abort();
#else
            /* //gr: avoid bogus relocs for empty (debug) sections */
            if (s1->sections[s->sh_info]->sh_flags & SHF_ALLOC)
                prepare_dynamic_rel(s1, s);
            else if (s1->do_debug)
                s->sh_size = s->data_offset;
#endif
        } else if (s1->do_debug || file_type == TCC_OUTPUT_OBJ || (s->sh_flags & SHF_ALLOC)
                   || i == (s1->nb_sections - 1)) {
            /* we output all sections if debug or object file */
            s->sh_size = s->data_offset;
        }
    }

    /* allocate program segment headers */
    phdr = tcc_mallocz(phnum * sizeof(ElfW(Phdr)));

    if (s1->output_format == TCC_OUTPUT_FORMAT_ELF) {
        file_offset = sizeof(ElfW(Ehdr)) + phnum * sizeof(ElfW(Phdr));
    } else {
        file_offset = 0;
    }
    if (phnum > 0) {
#ifdef TCC_TARGET_816
        abort();
#else
        /* compute section to program header mapping */
        if (s1->has_text_addr) {
            int a_offset, p_offset;
            addr = s1->text_addr;
            /* we ensure that (addr % ELF_PAGE_SIZE) == file_offset %
               ELF_PAGE_SIZE */
            a_offset = addr & (s1->section_align - 1);
            p_offset = file_offset & (s1->section_align - 1);
            if (a_offset < p_offset)
                a_offset += s1->section_align;
            file_offset += (a_offset - p_offset);
        } else {
            if (file_type == TCC_OUTPUT_DLL)
                addr = 0;
            else
                addr = ELF_START_ADDR;
            /* compute address after headers */
            addr += (file_offset & (s1->section_align - 1));
        }

        /* dynamic relocation table information, for .dynamic section */
        rel_size = 0;
        rel_addr = 0;

#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
        bss_addr = bss_size = 0;
#endif
        /* leave one program header for the program interpreter */
        ph = &phdr[0];
        if (interp)
            ph += 1 + HAVE_PHDR;

        for (j = 0; j < 2; j++) {
            ph->p_type = PT_LOAD;
            if (j == 0)
                ph->p_flags = PF_R | PF_X;
            else
                ph->p_flags = PF_R | PF_W;
            ph->p_align = s1->section_align;

            /* we do the following ordering: interp, symbol tables,
               relocations, progbits, nobits */
            /* XXX: do faster and simpler sorting */
            for (k = 0; k < 5; k++) {
                for (i = 1; i < s1->nb_sections; i++) {
                    s = s1->sections[i];
                    /* compute if section should be included */
                    if (j == 0) {
                        if ((s->sh_flags & (SHF_ALLOC | SHF_WRITE)) != SHF_ALLOC)
                            continue;
                    } else {
                        if ((s->sh_flags & (SHF_ALLOC | SHF_WRITE)) != (SHF_ALLOC | SHF_WRITE))
                            continue;
                    }
                    if (s == interp) {
                        if (k != 0)
                            continue;
                    } else if (s->sh_type == SHT_DYNSYM || s->sh_type == SHT_STRTAB
                               || s->sh_type == SHT_HASH) {
                        if (k != 1)
                            continue;
                    } else if (s->sh_type == SHT_RELX) {
                        if (k != 2)
                            continue;
                    } else if (s->sh_type == SHT_NOBITS) {
                        if (k != 4)
                            continue;
                    } else {
                        if (k != 3)
                            continue;
                    }
                    section_order[sh_order_index++] = i;

                    /* section matches: we align it and add its size */
                    tmp = addr;
                    addr = (addr + s->sh_addralign - 1) & ~(s->sh_addralign - 1);
                    file_offset += addr - tmp;
                    s->sh_offset = file_offset;
                    s->sh_addr = addr;

                    /* update program header infos */
                    if (ph->p_offset == 0) {
                        ph->p_offset = file_offset;
                        ph->p_vaddr = addr;
                        ph->p_paddr = ph->p_vaddr;
                    }
                    /* update dynamic relocation infos */
                    if (s->sh_type == SHT_RELX) {
#if defined(__FreeBSD__)
                        if (!strcmp(strsec->data + s->sh_name, ".rel.got")) { // rel_size == 0) {
                            rel_addr = addr;
                            rel_size += s->sh_size; // XXX only first rel.
                        }
                        if (!strcmp(strsec->data + s->sh_name, ".rel.bss")) { // rel_size == 0) {
                            bss_addr = addr;
                            bss_size = s->sh_size; // XXX only first rel.
                        }
#else
                        if (rel_size == 0)
                            rel_addr = addr;
                        rel_size += s->sh_size;
#endif
                    }
                    addr += s->sh_size;
                    if (s->sh_type != SHT_NOBITS)
                        file_offset += s->sh_size;
                }
            }
            ph->p_filesz = file_offset - ph->p_offset;
            ph->p_memsz = addr - ph->p_vaddr;
            ph++;
            if (j == 0) {
                if (s1->output_format == TCC_OUTPUT_FORMAT_ELF) {
                    /* if in the middle of a page, we duplicate the page in
                       memory so that one copy is RX and the other is RW */
                    if ((addr & (s1->section_align - 1)) != 0)
                        addr += s1->section_align;
                } else {
                    addr = (addr + s1->section_align - 1) & ~(s1->section_align - 1);
                    file_offset = (file_offset + s1->section_align - 1) & ~(s1->section_align - 1);
                }
            }
        }

        /* if interpreter, then add corresponing program header */
        if (interp) {
            ph = &phdr[0];

#if defined(__FreeBSD__)
            {
                int len = phnum * sizeof(ElfW(Phdr));

                ph->p_type = PT_PHDR;
                ph->p_offset = sizeof(ElfW(Ehdr));
                ph->p_vaddr = interp->sh_addr - len;
                ph->p_paddr = ph->p_vaddr;
                ph->p_filesz = ph->p_memsz = len;
                ph->p_flags = PF_R | PF_X;
                ph->p_align = 4; // interp->sh_addralign;
                ph++;
            }
#endif

            ph->p_type = PT_INTERP;
            ph->p_offset = interp->sh_offset;
            ph->p_vaddr = interp->sh_addr;
            ph->p_paddr = ph->p_vaddr;
            ph->p_filesz = interp->sh_size;
            ph->p_memsz = interp->sh_size;
            ph->p_flags = PF_R;
            ph->p_align = interp->sh_addralign;
        }

        /* if dynamic section, then add corresponing program header */
        if (dynamic) {
            ElfW(TokenSym) * sym_end;

            ph = &phdr[phnum - 1];

            ph->p_type = PT_DYNAMIC;
            ph->p_offset = dynamic->sh_offset;
            ph->p_vaddr = dynamic->sh_addr;
            ph->p_paddr = ph->p_vaddr;
            ph->p_filesz = dynamic->sh_size;
            ph->p_memsz = dynamic->sh_size;
            ph->p_flags = PF_R | PF_W;
            ph->p_align = dynamic->sh_addralign;

            /* put GOT dynamic section address */
            put32(s1->got->data, dynamic->sh_addr);

            /* relocate the PLT */
            if (file_type == TCC_OUTPUT_EXE
#if defined(TCC_OUTPUT_DLL_WITH_PLT)
                || file_type == TCC_OUTPUT_DLL
#endif
            ) {
                uint8_t *p, *p_end;

                p = s1->plt->data;
                p_end = p + s1->plt->data_offset;
                if (p < p_end) {
#if defined(TCC_TARGET_I386)
                    put32(p + 2, get32(p + 2) + s1->got->sh_addr);
                    put32(p + 8, get32(p + 8) + s1->got->sh_addr);
                    p += 16;
                    while (p < p_end) {
                        put32(p + 2, get32(p + 2) + s1->got->sh_addr);
                        p += 16;
                    }
#elif defined(TCC_TARGET_X86_64)
                    int x = s1->got->sh_addr - s1->plt->sh_addr - 6;
                    put32(p + 2, get32(p + 2) + x);
                    put32(p + 8, get32(p + 8) + x - 6);
                    p += 16;
                    while (p < p_end) {
                        put32(p + 2, get32(p + 2) + x + s1->plt->data - p);
                        p += 16;
                    }
#elif defined(TCC_TARGET_ARM)
                    int x;
                    x = s1->got->sh_addr - s1->plt->sh_addr - 12;
                    p += 16;
                    while (p < p_end) {
                        put32(p + 12, x + get32(p + 12) + s1->plt->data - p);
                        p += 16;
                    }
#elif defined(TCC_TARGET_C67)
                    /* XXX: TODO */
#else
#error unsupported CPU
#endif
                }
            }

            /* relocate symbols in .dynsym */
            sym_end = (ElfW(TokenSym) *) (s1->dynsym->data + s1->dynsym->data_offset);
            for (sym = (ElfW(TokenSym) *) s1->dynsym->data + 1; sym < sym_end; sym++) {
                if (sym->st_shndx == SHN_UNDEF) {
                    /* relocate to the PLT if the symbol corresponds
                       to a PLT entry */
                    if (sym->st_value)
                        sym->st_value += s1->plt->sh_addr;
                } else if (sym->st_shndx < SHN_LORESERVE) {
                    /* do symbol relocation */
                    sym->st_value += s1->sections[sym->st_shndx]->sh_addr;
                }
            }

            /* put dynamic section entries */
            dynamic->data_offset = saved_dynamic_data_offset;
            put_dt(dynamic, DT_HASH, s1->dynsym->hash->sh_addr);
            put_dt(dynamic, DT_STRTAB, dynstr->sh_addr);
            put_dt(dynamic, DT_SYMTAB, s1->dynsym->sh_addr);
            put_dt(dynamic, DT_STRSZ, dynstr->data_offset);
            put_dt(dynamic, DT_SYMENT, sizeof(ElfW(TokenSym)));
#ifdef TCC_TARGET_X86_64
            put_dt(dynamic, DT_RELA, rel_addr);
            put_dt(dynamic, DT_RELASZ, rel_size);
            put_dt(dynamic, DT_RELAENT, sizeof(ElfW_Rel));
#else
#if defined(__FreeBSD__)
            put_dt(dynamic, DT_PLTGOT, s1->got->sh_addr);
            put_dt(dynamic, DT_PLTRELSZ, rel_size);
            put_dt(dynamic, DT_JMPREL, rel_addr);
            put_dt(dynamic, DT_PLTREL, DT_REL);
            put_dt(dynamic, DT_REL, bss_addr);
            put_dt(dynamic, DT_RELSZ, bss_size);
#else
            put_dt(dynamic, DT_REL, rel_addr);
            put_dt(dynamic, DT_RELSZ, rel_size);
            put_dt(dynamic, DT_RELENT, sizeof(ElfW_Rel));
#endif
#endif
            if (s1->do_debug)
                put_dt(dynamic, DT_DEBUG, 0);
            put_dt(dynamic, DT_NULL, 0);
        }

        ehdr.e_phentsize = sizeof(ElfW(Phdr));
        ehdr.e_phnum = phnum;
        ehdr.e_phoff = sizeof(ElfW(Ehdr));
#endif
    }

    /* all other sections come after */
    for (i = 1; i < s1->nb_sections; i++) {
        s = s1->sections[i];
        if (phnum > 0 && (s->sh_flags & SHF_ALLOC))
            continue;
        section_order[sh_order_index++] = i;

        file_offset = (file_offset + s->sh_addralign - 1) & ~(s->sh_addralign - 1);
        s->sh_offset = file_offset;
        if (s->sh_type != SHT_NOBITS)
            file_offset += s->sh_size;
    }

#ifndef TCC_TARGET_816
    /* if building executable or DLL, then relocate each section
       except the GOT which is already relocated */
    if (file_type != TCC_OUTPUT_OBJ) {
        relocate_syms(s1, 0);

        if (s1->nb_errors != 0) {
        fail:
            ret = -1;
            goto the_end;
        }

        /* relocate sections */
        /* XXX: ignore sections with allocated relocations ? */
        for (i = 1; i < s1->nb_sections; i++) {
            s = s1->sections[i];
            if (s->reloc && s != s1->got)
                relocate_section(s1, s);
        }

        /* relocate relocation entries if the relocation tables are
           allocated in the executable */
        for (i = 1; i < s1->nb_sections; i++) {
            s = s1->sections[i];
            if ((s->sh_flags & SHF_ALLOC) && s->sh_type == SHT_RELX) {
                relocate_rel(s1, s);
            }
        }

        /* get entry point address */
        if (file_type == TCC_OUTPUT_EXE)
            ehdr.e_entry = (uplong) tcc_get_symbol_err(s1, "_start");
        else
            ehdr.e_entry = text_section->sh_addr; /* XXX: is it correct ? */
    }
#endif

    /* write elf file */
    if (file_type == TCC_OUTPUT_OBJ)
        mode = 0666;
    else
        mode = 0777;
    fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, mode);
    if (fd < 0) {
        error_noabort("could not write '%s'", filename);
#ifdef TCC_TARGET_816
        ret = -1;
        goto the_end;
#else
        goto fail;
#endif
    }
    f = fdopen(fd, "wb");
    if (s1->verbose)
        printf("<- %s\n", filename);

#ifdef TCC_TARGET_COFF
    if (s1->output_format == TCC_OUTPUT_FORMAT_COFF) {
        tcc_output_coff(s1, f);
    } else
#endif
        if (s1->output_format == TCC_OUTPUT_FORMAT_ELF) {
#ifdef TCC_TARGET_816
        abort();
#else
        sort_syms(s1, symtab_section);

        /* align to 4 */
        file_offset = (file_offset + 3) & -4;

        /* fill header */
        ehdr.e_ident[0] = ELFMAG0;
        ehdr.e_ident[1] = ELFMAG1;
        ehdr.e_ident[2] = ELFMAG2;
        ehdr.e_ident[3] = ELFMAG3;
        ehdr.e_ident[4] = TCC_ELFCLASS;
        ehdr.e_ident[5] = ELFDATA2LSB;
        ehdr.e_ident[6] = EV_CURRENT;
#ifdef __FreeBSD__
        ehdr.e_ident[EI_OSABI] = ELFOSABI_FREEBSD;
#endif
#ifdef TCC_TARGET_ARM
#ifdef TCC_ARM_EABI
        ehdr.e_ident[EI_OSABI] = 0;
        ehdr.e_flags = 4 << 24;
#else
        ehdr.e_ident[EI_OSABI] = ELFOSABI_ARM;
#endif
#endif
        switch (file_type) {
        default:
        case TCC_OUTPUT_EXE:
            ehdr.e_type = ET_EXEC;
            break;
        case TCC_OUTPUT_DLL:
            ehdr.e_type = ET_DYN;
            break;
        case TCC_OUTPUT_OBJ:
            ehdr.e_type = ET_REL;
            break;
        }
        ehdr.e_machine = EM_TCC_TARGET;
        ehdr.e_version = EV_CURRENT;
        ehdr.e_shoff = file_offset;
        ehdr.e_ehsize = sizeof(ElfW(Ehdr));
        ehdr.e_shentsize = sizeof(ElfW(Shdr));
        ehdr.e_shnum = shnum;
        ehdr.e_shstrndx = shnum - 1;

        fwrite(&ehdr, 1, sizeof(ElfW(Ehdr)), f);
        fwrite(phdr, 1, phnum * sizeof(ElfW(Phdr)), f);
        offset = sizeof(ElfW(Ehdr)) + phnum * sizeof(ElfW(Phdr));

        for (i = 1; i < s1->nb_sections; i++) {
            s = s1->sections[section_order[i]];
            if (s->sh_type != SHT_NOBITS) {
#if defined(__FreeBSD__)
                if (s->sh_type == SHT_DYNSYM)
                    patch_dynsym_undef(s1, s);
#endif
                while (offset < s->sh_offset) {
                    fputc(0, f);
                    offset++;
                }
                size = s->sh_size;
                fwrite(s->data, 1, size, f);
                offset += size;
            }
        }

        /* output section headers */
        while (offset < ehdr.e_shoff) {
            fputc(0, f);
            offset++;
        }

        for (i = 0; i < s1->nb_sections; i++) {
            sh = &shdr;
            memset(sh, 0, sizeof(ElfW(Shdr)));
            s = s1->sections[i];
            if (s) {
                sh->sh_name = s->sh_name;
                sh->sh_type = s->sh_type;
                sh->sh_flags = s->sh_flags;
                sh->sh_entsize = s->sh_entsize;
                sh->sh_info = s->sh_info;
                if (s->link)
                    sh->sh_link = s->link->sh_num;
                sh->sh_addralign = s->sh_addralign;
                sh->sh_addr = s->sh_addr;
                sh->sh_offset = s->sh_offset;
                sh->sh_size = s->sh_size;
            }
            fwrite(sh, 1, sizeof(ElfW(Shdr)), f);
        }
#endif
    } else {
        tcc_output_binary(s1, f, section_order);
    }
    fclose(f);

    ret = 0;
the_end:
    tcc_free(s1->symtab_to_dynsym);
    tcc_free(section_order);
    tcc_free(phdr);
    tcc_free(s1->got_offsets);
    return ret;
}

/**
 * @brief Output the compiled code to a file.
 *
 * This function writes the compiled code to the specified file based on the
 * output type specified in the TCCState structure.
 *
 * @param s         The TCC state structure.
 * @param filename  The name of the output file.
 * @return          0 on success, -1 on failure.
 */
int tcc_output_file(TCCState *s, const char *filename)
{
    int ret;
#ifdef TCC_TARGET_PE
    if (s->output_type != TCC_OUTPUT_OBJ) {
        ret = pe_output_file(s, filename);
    } else
#endif
    {
        ret = elf_output_file(s, filename);
    }
    return ret;
}

#ifndef TCC_TARGET_816
/**
 * @brief Load data from a file into memory.
 *
 * This function loads the data from the specified file descriptor starting at
 * the specified file offset into memory. It allocates memory to store the data
 * and returns a pointer to the allocated memory.
 *
 * @param fd           The file descriptor of the file.
 * @param file_offset  The offset within the file to start reading from.
 * @param size         The size of the data to read.
 * @return             A pointer to the loaded data, or NULL on failure.
 */
static void *load_data(int fd, unsigned long file_offset, unsigned long size)
{
    void *data;

    data = tcc_malloc(size);
    lseek(fd, file_offset, SEEK_SET);
    read(fd, data, size);
    return data;
}

/**
 * @brief Information about section merging during linking.
 *
 * This structure contains information about the merging of sections during
 * the linking process. It is used to track the corresponding existing section,
 * the offset of the new section within the existing section, and other
 * attributes related to the merging process.
 */
typedef struct SectionMergeInfo
{
    Section *s;           /**< Corresponding existing section. */
    unsigned long offset; /**< Offset of the new section in the existing section. */
    uint8_t new_section;  /**< Indicates if section 's' was added. */
    uint8_t link_once;    /**< Indicates if the section is a link once section. */
} SectionMergeInfo;

/**
 * @brief Load an object file and merge it with the current files.
 *
 * This function loads an object file from the given file descriptor and merges
 * it with the current files being compiled. The object file is assumed to start
 * at the specified file offset.
 *
 * @param s1           The TCC compilation state.
 * @param fd           The file descriptor of the object file.
 * @param file_offset  The offset within the file where the object file starts.
 * @return             Zero on success, or a non-zero value if an error occurred.
 *
 * @note This function does not currently handle stab (debug) information.
 */
static int tcc_load_object_file(TCCState *s1, int fd, unsigned long file_offset)
{
    ElfW(Ehdr) ehdr;
    ElfW(Shdr) * shdr, *sh;
    int size, i, j, offset, offseti, nb_syms, sym_index, ret;
    unsigned char *strsec, *strtab;
    int *old_to_new_syms;
    char *sh_name, *name;
    SectionMergeInfo *sm_table, *sm;
    ElfW(TokenSym) * sym, *symtab;
    ElfW_Rel *rel, *rel_end;
    Section *s;

    int stab_index;
    int stabstr_index;

    stab_index = stabstr_index = 0;

    if (read(fd, &ehdr, sizeof(ehdr)) != sizeof(ehdr))
        goto fail1;
    if (ehdr.e_ident[0] != ELFMAG0 || ehdr.e_ident[1] != ELFMAG1 || ehdr.e_ident[2] != ELFMAG2
        || ehdr.e_ident[3] != ELFMAG3)
        goto fail1;
    /* test if object file */
    if (ehdr.e_type != ET_REL)
        goto fail1;
    /* test CPU specific stuff */
    if (ehdr.e_ident[5] != ELFDATA2LSB || ehdr.e_machine != EM_TCC_TARGET) {
    fail1:
        error_noabort("invalid object file");
        return -1;
    }
    /* read sections */
    shdr = load_data(fd, file_offset + ehdr.e_shoff, sizeof(ElfW(Shdr)) * ehdr.e_shnum);
    sm_table = tcc_mallocz(sizeof(SectionMergeInfo) * ehdr.e_shnum);

    /* load section names */
    sh = &shdr[ehdr.e_shstrndx];
    strsec = load_data(fd, file_offset + sh->sh_offset, sh->sh_size);

    /* load symtab and strtab */
    old_to_new_syms = NULL;
    symtab = NULL;
    strtab = NULL;
    nb_syms = 0;
    for (i = 1; i < ehdr.e_shnum; i++) {
        sh = &shdr[i];
        if (sh->sh_type == SHT_SYMTAB) {
            if (symtab) {
                error_noabort("object must contain only one symtab");
            fail:
                ret = -1;
                goto the_end;
            }
            nb_syms = sh->sh_size / sizeof(ElfW(TokenSym));
            symtab = load_data(fd, file_offset + sh->sh_offset, sh->sh_size);
            sm_table[i].s = symtab_section;

            /* now load strtab */
            sh = &shdr[sh->sh_link];
            strtab = load_data(fd, file_offset + sh->sh_offset, sh->sh_size);
        }
    }

    /* now examine each section and try to merge its content with the
       ones in memory */
    for (i = 1; i < ehdr.e_shnum; i++) {
        /* no need to examine section name strtab */
        if (i == ehdr.e_shstrndx)
            continue;
        sh = &shdr[i];
        sh_name = strsec + sh->sh_name;
        /* ignore sections types we do not handle */
        if (sh->sh_type != SHT_PROGBITS && sh->sh_type != SHT_RELX &&
#ifdef TCC_ARM_EABI
            sh->sh_type != SHT_ARM_EXIDX &&
#endif
            sh->sh_type != SHT_NOBITS && strcmp(sh_name, ".stabstr"))
            continue;
        if (sh->sh_addralign < 1)
            sh->sh_addralign = 1;
        /* find corresponding section, if any */
        for (j = 1; j < s1->nb_sections; j++) {
            s = s1->sections[j];
            if (!strcmp(s->name, sh_name)) {
                if (!strncmp(sh_name, ".gnu.linkonce", sizeof(".gnu.linkonce") - 1)) {
                    /* if a 'linkonce' section is already present, we
                       do not add it again. It is a little tricky as
                       symbols can still be defined in
                       it. */
                    sm_table[i].link_once = 1;
                    goto next;
                } else {
                    goto found;
                }
            }
        }
        /* not found: create new section */
        s = new_section(s1, sh_name, sh->sh_type, sh->sh_flags);
        /* take as much info as possible from the section. sh_link and
           sh_info will be updated later */
        s->sh_addralign = sh->sh_addralign;
        s->sh_entsize = sh->sh_entsize;
        sm_table[i].new_section = 1;
    found:
        if (sh->sh_type != s->sh_type) {
            error_noabort("invalid section type");
            goto fail;
        }

        /* align start of section */
        offset = s->data_offset;

        if (0 == strcmp(sh_name, ".stab")) {
            stab_index = i;
            goto no_align;
        }
        if (0 == strcmp(sh_name, ".stabstr")) {
            stabstr_index = i;
            goto no_align;
        }

        size = sh->sh_addralign - 1;
        offset = (offset + size) & ~size;
        if (sh->sh_addralign > s->sh_addralign)
            s->sh_addralign = sh->sh_addralign;
        s->data_offset = offset;
    no_align:
        sm_table[i].offset = offset;
        sm_table[i].s = s;
        /* concatenate sections */
        size = sh->sh_size;
        if (sh->sh_type != SHT_NOBITS) {
            unsigned char *ptr;
            lseek(fd, file_offset + sh->sh_offset, SEEK_SET);
            ptr = section_ptr_add(s, size);
            read(fd, ptr, size);
        } else {
            s->data_offset += size;
        }
    next:;
    }

    /* //gr relocate stab strings */
    if (stab_index && stabstr_index) {
        Stab_Sym *a, *b;
        unsigned o;
        s = sm_table[stab_index].s;
        a = (Stab_Sym *) (s->data + sm_table[stab_index].offset);
        b = (Stab_Sym *) (s->data + s->data_offset);
        o = sm_table[stabstr_index].offset;
        while (a < b)
            a->n_strx += o, a++;
    }

    /* second short pass to update sh_link and sh_info fields of new
       sections */
    for (i = 1; i < ehdr.e_shnum; i++) {
        s = sm_table[i].s;
        if (!s || !sm_table[i].new_section)
            continue;
        sh = &shdr[i];
        if (sh->sh_link > 0)
            s->link = sm_table[sh->sh_link].s;
        if (sh->sh_type == SHT_RELX) {
            s->sh_info = sm_table[sh->sh_info].s->sh_num;
            /* update backward link */
            s1->sections[s->sh_info]->reloc = s;
        }
    }
    sm = sm_table;

    /* resolve symbols */
    old_to_new_syms = tcc_mallocz(nb_syms * sizeof(int));

    sym = symtab + 1;
    for (i = 1; i < nb_syms; i++, sym++) {
        if (sym->st_shndx != SHN_UNDEF && sym->st_shndx < SHN_LORESERVE) {
            sm = &sm_table[sym->st_shndx];
            if (sm->link_once) {
                /* if a symbol is in a link once section, we use the
                   already defined symbol. It is very important to get
                   correct relocations */
                if (ELFW(ST_BIND)(sym->st_info) != STB_LOCAL) {
                    name = strtab + sym->st_name;
                    sym_index = find_elf_sym(symtab_section, name);
                    if (sym_index)
                        old_to_new_syms[i] = sym_index;
                }
                continue;
            }
            /* if no corresponding section added, no need to add symbol */
            if (!sm->s)
                continue;
            /* convert section number */
            sym->st_shndx = sm->s->sh_num;
            /* offset value */
            sym->st_value += sm->offset;
        }
        /* add symbol */
        name = strtab + sym->st_name;
        sym_index = add_elf_sym(symtab_section,
                                sym->st_value,
                                sym->st_size,
                                sym->st_info,
                                sym->st_other,
                                sym->st_shndx,
                                name);
        old_to_new_syms[i] = sym_index;
    }

    /* third pass to patch relocation entries */
    for (i = 1; i < ehdr.e_shnum; i++) {
        s = sm_table[i].s;
        if (!s)
            continue;
        sh = &shdr[i];
        offset = sm_table[i].offset;
        switch (s->sh_type) {
        case SHT_RELX:
            /* take relocation offset information */
            offseti = sm_table[sh->sh_info].offset;
            rel_end = (ElfW_Rel *) (s->data + s->data_offset);
            for (rel = (ElfW_Rel *) (s->data + offset); rel < rel_end; rel++) {
                int type;
                unsigned sym_index;
                /* convert symbol index */
                type = ELFW(R_TYPE)(rel->r_info);
                sym_index = ELFW(R_SYM)(rel->r_info);
                /* NOTE: only one symtab assumed */
                if (sym_index >= nb_syms)
                    goto invalid_reloc;
                sym_index = old_to_new_syms[sym_index];
                /* ignore link_once in rel section. */
                if (!sym_index && !sm->link_once) {
                invalid_reloc:
                    error_noabort("Invalid relocation entry [%2d] '%s' @ %.8x",
                                  i,
                                  strsec + sh->sh_name,
                                  rel->r_offset);
                    goto fail;
                }
                rel->r_info = ELFW(R_INFO)(sym_index, type);
                /* offset the relocation offset */
                rel->r_offset += offseti;
            }
            break;
        default:
            break;
        }
    }

    ret = 0;
the_end:
    tcc_free(symtab);
    tcc_free(strtab);
    tcc_free(old_to_new_syms);
    tcc_free(sm_table);
    tcc_free(strsec);
    tcc_free(shdr);
    return ret;
}

#define ARMAG "!<arch>\012" /* For COFF and a.out archives */

/**
 * @brief Archive file header format.
 *
 * The `ArchiveHeader` structure represents the header format used in an archive
 * file.
 */
typedef struct ArchiveHeader
{
    char ar_name[16]; /**< Name of this member. */
    char ar_date[12]; /**< File modification time. */
    char ar_uid[6];   /**< Owner UID, printed as decimal. */
    char ar_gid[6];   /**< Owner GID, printed as decimal. */
    char ar_mode[8];  /**< File mode, printed as octal. */
    char ar_size[10]; /**< File size, printed as decimal. */
    char ar_fmag[2];  /**< Should contain ARFMAG. */
} ArchiveHeader;

/**
 * @brief Read a 32-bit big-endian integer from a byte array.
 *
 * The `get_be32` function reads a 32-bit big-endian integer from a byte array
 * and returns the corresponding value.
 *
 * @param b The byte array.
 * @return The 32-bit big-endian integer value.
 */
static int get_be32(const uint8_t *b)
{
    return b[3] | (b[2] << 8) | (b[1] << 16) | (b[0] << 24);
}

/**
 * @brief Load objects from an archive file that resolve undefined symbols.
 *
 * The `tcc_load_alacarte` function loads only the objects from an archive file
 * that resolve undefined symbols. It iterates over the symbols in the symbol
 * table and checks if they are undefined. If an undefined symbol is found,
 * the corresponding object file is loaded and merged into the current files.
 *
 * @param s1   The TCC state.
 * @param fd   The file descriptor of the archive file.
 * @param size The size of the archive file.
 * @return 0 on success, -1 on failure.
 */
static int tcc_load_alacarte(TCCState *s1, int fd, int size)
{
    int i, bound, nsyms, sym_index, off, ret;
    uint8_t *data;
    const char *ar_names, *p;
    const uint8_t *ar_index;
    ElfW(TokenSym) * sym;

    data = tcc_malloc(size);
    if (read(fd, data, size) != size)
        goto fail;
    nsyms = get_be32(data);
    ar_index = data + 4;
    ar_names = ar_index + nsyms * 4;

    do {
        bound = 0;
        for (p = ar_names, i = 0; i < nsyms; i++, p += strlen(p) + 1) {
            sym_index = find_elf_sym(symtab_section, p);
            if (sym_index) {
                sym = &((ElfW(TokenSym) *) symtab_section->data)[sym_index];
                if (sym->st_shndx == SHN_UNDEF) {
                    off = get_be32(ar_index + i * 4) + sizeof(ArchiveHeader);
#if 0
                    printf("%5d\t%s\t%08x\n", i, p, sym->st_shndx);
#endif
                    ++bound;
                    lseek(fd, off, SEEK_SET);
                    if (tcc_load_object_file(s1, fd, off) < 0) {
                    fail:
                        ret = -1;
                        goto the_end;
                    }
                }
            }
        }
    } while (bound);
    ret = 0;
the_end:
    tcc_free(data);
    return ret;
}

/**
 * @brief Load an archive file (.a) and process its contents.
 *
 * The `tcc_load_archive` function reads an archive file and processes its
 * contents. It iterates over the members of the archive and loads the object
 * files or handles special cases such as coff symbol tables. The function
 * supports both standard archives and alacarte archives.
 *
 * @param s1 The TCC state.
 * @param fd The file descriptor of the archive file.
 * @return 0 on success, -1 on failure.
 */
static int tcc_load_archive(TCCState *s1, int fd)
{
    ArchiveHeader hdr;
    char ar_size[11];
    char ar_name[17];
    char magic[8];
    int size, len, i;
    unsigned long file_offset;

    /* skip magic which was already checked */
    read(fd, magic, sizeof(magic));

    for (;;) {
        len = read(fd, &hdr, sizeof(hdr));
        if (len == 0)
            break;
        if (len != sizeof(hdr)) {
            error_noabort("invalid archive");
            return -1;
        }
        memcpy(ar_size, hdr.ar_size, sizeof(hdr.ar_size));
        ar_size[sizeof(hdr.ar_size)] = '\0';
        size = strtol(ar_size, NULL, 0);
        memcpy(ar_name, hdr.ar_name, sizeof(hdr.ar_name));
        for (i = sizeof(hdr.ar_name) - 1; i >= 0; i--) {
            if (ar_name[i] != ' ')
                break;
        }
        ar_name[i + 1] = '\0';
        //        printf("name='%s' size=%d %s\n", ar_name, size, ar_size);
        file_offset = lseek(fd, 0, SEEK_CUR);
        /* align to even */
        size = (size + 1) & ~1;
        if (!strcmp(ar_name, "/")) {
            /* coff symbol table : we handle it */
            if (s1->alacarte_link)
                return tcc_load_alacarte(s1, fd, size);
        } else if (!strcmp(ar_name, "//") || !strcmp(ar_name, "__.SYMDEF")
                   || !strcmp(ar_name, "__.SYMDEF/") || !strcmp(ar_name, "ARFILENAMES/")) {
            /* skip symbol table or archive names */
        } else {
            if (tcc_load_object_file(s1, fd, file_offset) < 0)
                return -1;
        }
        lseek(fd, file_offset + size, SEEK_SET);
    }
    return 0;
}

#ifndef TCC_TARGET_PE
/**
 * @brief Load a DLL file and all referenced DLLs.
 *
 * The `tcc_load_dll` function reads a DLL file and processes its dynamic
 * sections and symbols. It also loads all referenced DLLs recursively. The
 * function adds dynamic symbols to the dynsym_section and updates the
 * loaded_dlls list with the loaded DLLs and their levels.
 *
 * @param s1 The TCC state.
 * @param fd The file descriptor of the DLL file.
 * @param filename The name of the DLL file.
 * @param level The level of the DLL file (0 if referenced by the user).
 * @return 0 on success, -1 on failure.
 */
static int tcc_load_dll(TCCState *s1, int fd, const char *filename, int level)
{
    ElfW(Ehdr) ehdr;
    ElfW(Shdr) * shdr, *sh, *sh1;
    int i, j, nb_syms, nb_dts, sym_bind, ret;
    ElfW(TokenSym) * sym, *dynsym;
    ElfW(Dyn) * dt, *dynamic;
    unsigned char *dynstr;
    const char *name, *soname;
    DLLReference *dllref;

    read(fd, &ehdr, sizeof(ehdr));

    /* test CPU specific stuff */
    if (ehdr.e_ident[5] != ELFDATA2LSB || ehdr.e_machine != EM_TCC_TARGET) {
        error_noabort("bad architecture");
        return -1;
    }

    /* read sections */
    shdr = load_data(fd, ehdr.e_shoff, sizeof(ElfW(Shdr)) * ehdr.e_shnum);

    /* load dynamic section and dynamic symbols */
    nb_syms = 0;
    nb_dts = 0;
    dynamic = NULL;
    dynsym = NULL; /* avoid warning */
    dynstr = NULL; /* avoid warning */
    for (i = 0, sh = shdr; i < ehdr.e_shnum; i++, sh++) {
        switch (sh->sh_type) {
        case SHT_DYNAMIC:
            nb_dts = sh->sh_size / sizeof(ElfW(Dyn));
            dynamic = load_data(fd, sh->sh_offset, sh->sh_size);
            break;
        case SHT_DYNSYM:
            nb_syms = sh->sh_size / sizeof(ElfW(TokenSym));
            dynsym = load_data(fd, sh->sh_offset, sh->sh_size);
            sh1 = &shdr[sh->sh_link];
            dynstr = load_data(fd, sh1->sh_offset, sh1->sh_size);
            break;
        default:
            break;
        }
    }

    /* compute the real library name */
    soname = tcc_basename(filename);

    for (i = 0, dt = dynamic; i < nb_dts; i++, dt++) {
        if (dt->d_tag == DT_SONAME) {
            soname = dynstr + dt->d_un.d_val;
        }
    }

    /* if the dll is already loaded, do not load it */
    for (i = 0; i < s1->nb_loaded_dlls; i++) {
        dllref = s1->loaded_dlls[i];
        if (!strcmp(soname, dllref->name)) {
            /* but update level if needed */
            if (level < dllref->level)
                dllref->level = level;
            ret = 0;
            goto the_end;
        }
    }

    //    printf("loading dll '%s'\n", soname);

    /* add the dll and its level */
    dllref = tcc_mallocz(sizeof(DLLReference) + strlen(soname));
    dllref->level = level;
    strcpy(dllref->name, soname);
    dynarray_add((void ***) &s1->loaded_dlls, &s1->nb_loaded_dlls, dllref);

    /* add dynamic symbols in dynsym_section */
    for (i = 1, sym = dynsym + 1; i < nb_syms; i++, sym++) {
        sym_bind = ELFW(ST_BIND)(sym->st_info);
        if (sym_bind == STB_LOCAL)
            continue;
        name = dynstr + sym->st_name;
        add_elf_sym(s1->dynsymtab_section,
                    sym->st_value,
                    sym->st_size,
                    sym->st_info,
                    sym->st_other,
                    sym->st_shndx,
                    name);
    }

    /* load all referenced DLLs */
    for (i = 0, dt = dynamic; i < nb_dts; i++, dt++) {
        switch (dt->d_tag) {
        case DT_NEEDED:
            name = dynstr + dt->d_un.d_val;
            for (j = 0; j < s1->nb_loaded_dlls; j++) {
                dllref = s1->loaded_dlls[j];
                if (!strcmp(name, dllref->name))
                    goto already_loaded;
            }
            if (tcc_add_dll(s1, name, AFF_REFERENCED_DLL) < 0) {
                error_noabort("referenced dll '%s' not found", name);
                ret = -1;
                goto the_end;
            }
        already_loaded:
            break;
        }
    }
    ret = 0;
the_end:
    tcc_free(dynstr);
    tcc_free(dynsym);
    tcc_free(dynamic);
    tcc_free(shdr);
    return ret;
}

#define LD_TOK_NAME 256
#define LD_TOK_EOF (-1)

/**
 * @brief Get the next ld script token.
 *
 * The `ld_next` function reads characters from the input file and determines the
 * next ld script token based on the character. It handles whitespace, comments,
 * and different types of names.
 *
 * @param s1 The TCC state.
 * @param name The buffer to store the name of the token.
 * @param name_size The size of the name buffer.
 * @return The type of the token.
 */
static int ld_next(TCCState *s1, char *name, int name_size)
{
    int c;
    char *q;

redo:
    switch (ch) {
    case ' ':
    case '\t':
    case '\f':
    case '\v':
    case '\r':
    case '\n':
        inp();
        goto redo;
    case '/':
        minp();
        if (ch == '*') {
            file->buf_ptr = parse_comment(file->buf_ptr);
            ch = file->buf_ptr[0];
            goto redo;
        } else {
            q = name;
            *q++ = '/';
            goto parse_name;
        }
        break;
    /* case 'a' ... 'z': */
    case 'a':
    case 'b':
    case 'c':
    case 'd':
    case 'e':
    case 'f':
    case 'g':
    case 'h':
    case 'i':
    case 'j':
    case 'k':
    case 'l':
    case 'm':
    case 'n':
    case 'o':
    case 'p':
    case 'q':
    case 'r':
    case 's':
    case 't':
    case 'u':
    case 'v':
    case 'w':
    case 'x':
    case 'y':
    case 'z':
    /* case 'A' ... 'z': */
    case 'A':
    case 'B':
    case 'C':
    case 'D':
    case 'E':
    case 'F':
    case 'G':
    case 'H':
    case 'I':
    case 'J':
    case 'K':
    case 'L':
    case 'M':
    case 'N':
    case 'O':
    case 'P':
    case 'Q':
    case 'R':
    case 'S':
    case 'T':
    case 'U':
    case 'V':
    case 'W':
    case 'X':
    case 'Y':
    case 'Z':
    case '_':
    case '\\':
    case '.':
    case '$':
    case '~':
        q = name;
    parse_name:
        for (;;) {
            if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')
                  || strchr("/.-_+=$:\\,~", ch)))
                break;
            if ((q - name) < name_size - 1) {
                *q++ = ch;
            }
            minp();
        }
        *q = '\0';
        c = LD_TOK_NAME;
        break;
    case CH_EOF:
        c = LD_TOK_EOF;
        break;
    default:
        c = ch;
        inp();
        break;
    }
#if 0
    printf("tok=%c %d\n", c, c);
    if (c == LD_TOK_NAME)
        printf("  name=%s\n", name);
#endif
    return c;
}

/**
 * @brief Add a list of files specified in an ld script to the TCC state.
 *
 * The `ld_add_file_list` function reads a list of filenames from the ld script
 * and adds the files to the TCC state. It supports the `AS_NEEDED` directive to
 * conditionally add files based on dependencies.
 *
 * @param s1 The TCC state.
 * @param as_needed Flag indicating whether the files are added as needed.
 * @return 0 on success, -1 on failure.
 */
static int ld_add_file_list(TCCState *s1, int as_needed)
{
    char filename[1024];
    int t, ret;

    t = ld_next(s1, filename, sizeof(filename));
    if (t != '(')
        expect("(");
    t = ld_next(s1, filename, sizeof(filename));
    for (;;) {
        if (t == LD_TOK_EOF) {
            error_noabort("unexpected end of file");
            return -1;
        } else if (t == ')') {
            break;
        } else if (t != LD_TOK_NAME) {
            error_noabort("filename expected");
            return -1;
        }
        if (!strcmp(filename, "AS_NEEDED")) {
            ret = ld_add_file_list(s1, 1);
            if (ret)
                return ret;
        } else {
            /* TODO: Implement AS_NEEDED support. Ignore it for now */
            if (!as_needed)
                tcc_add_file(s1, filename);
        }
        t = ld_next(s1, filename, sizeof(filename));
        if (t == ',') {
            t = ld_next(s1, filename, sizeof(filename));
        }
    }
    return 0;
}

/**
 * @brief Interpret a subset of GNU ldscripts to handle the dummy libc.so files.
 *
 * The `tcc_load_ldscript` function interprets a subset of GNU ldscripts to
 * handle the dummy `libc.so` files. It reads commands from the ld script and
 * processes them accordingly.
 *
 * @param s1 The TCC state.
 * @return 0 on success, -1 on failure.
 */
static int tcc_load_ldscript(TCCState *s1)
{
    char cmd[64];
    char filename[1024];
    int t, ret;

    ch = file->buf_ptr[0];
    ch = handle_eob();
    for (;;) {
        t = ld_next(s1, cmd, sizeof(cmd));
        if (t == LD_TOK_EOF)
            return 0;
        else if (t != LD_TOK_NAME)
            return -1;
        if (!strcmp(cmd, "INPUT") || !strcmp(cmd, "GROUP")) {
            ret = ld_add_file_list(s1, 0);
            if (ret)
                return ret;
        } else if (!strcmp(cmd, "OUTPUT_FORMAT") || !strcmp(cmd, "TARGET")) {
            /* ignore some commands */
            t = ld_next(s1, cmd, sizeof(cmd));
            if (t != '(')
                expect("(");
            for (;;) {
                t = ld_next(s1, filename, sizeof(filename));
                if (t == LD_TOK_EOF) {
                    error_noabort("unexpected end of file");
                    return -1;
                } else if (t == ')') {
                    break;
                }
            }
        } else {
            return -1;
        }
    }
    return 0;
}
#endif
#endif
