using System;
using System.Collections.Generic;
using System.ComponentModel.DataAnnotations;

using Melchior.Utilities.Attributes;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Avalonia.Media;

namespace tiledpalettequant.Program
{

    /// <summary>
    /// These are input settings for the application. NOTE*** The default values are taken from the original program code's defaults!
    /// </summary>
    public class Inputs
    {
        [DisplayNumber<int>(Name = "TileWidth", Description = "TBD", DefaultValue = 8, MinValue = 1, MaxValue = 32, CommandLineFlag = "TBD")]
        public int TileWidth {get;set;}

        [DisplayNumber<int>(Name = "TileHeight", Description = "TBD", DefaultValue = 8, MinValue = 1, MaxValue = 32, CommandLineFlag = "TBD")]
        public int TileHeight {get;set;}

        [DisplayNumber<int>(Name = "NumberOfPalettes", Description = "TBD", DefaultValue = 8, MinValue = 1, MaxValue = 16, CommandLineFlag = "TBD")]
        public int NumberOfPalettes { get; set; }

        [DisplayNumber<int>(Name = "ColorsPerPalette", Description = "TBD", DefaultValue = 4, MinValue = 2, MaxValue = 256, CommandLineFlag = "TBD")]
        public int ColorsPerPalette { get; set; }

        [DisplayNumber<int>(Name = "BitsPerChannel", 
                            Description = "5 bits per channel default. LOOK THIS ONE UP! It works usually.", 
                            DefaultValue = 5,
                            MinValue = 2, 
                            MaxValue = 8,
                            CommandLineFlag = "TBD")]
        public int BitsPerChannel { get; set; }

        [DisplayNumber<float>(Name = "FractionOfPixels",
                              Description = "TBD",
                              DefaultValue = 0.1f,
                              MinValue = 0.01f,
                              MaxValue = 10.0f,
                              StepValue = 0.01f,
                              CommandLineFlag = "TBD")]
        public int FractionOfPixels { get; set; }


        [DisplayExtended<ColorZeroBehavior>(Name = "ColorZeroIndexBehavior", DefaultValue = ColorZeroBehavior.Unique, Description = "TBD")]
        public ColorZeroBehavior ColorZeroIndexBehavior { get; set; }

        [DisplayExtended<Dither>(Name = "Dither", DefaultValue = Dither.Off, Description = "TBD")]
        public Dither Dither { get; set; }

        [DisplayExtended<DitherPattern>(Name = "DitherPattern", DefaultValue = DitherPattern.Diagonal4, Description = "TBD")]
        public DitherPattern DitherPattern { get; set; }

        [DisplayExtended<string>(Name = "SharedColor", DefaultValue = "#000000", Description = "TBD")]
        public string SharedColor { get; set; }

        [DisplayExtended<string>(Name = "TransparentColor", DefaultValue = "#000000", Description = "TBD")]
        public string TransparentColor { get; set; }

        [DisplayNumber<float>(Name = "DitherWeight", Description = "TBD", DefaultValue = 0.5f, MaxValue = 1.0f, MinValue = 0.01f, StepValue = 0.01f)]
        public float DitherWeight { get; set; }

        public Inputs()
        {
            this.SharedColor = "#000000";
            this.TransparentColor = "#000000";
        }

    }
}
