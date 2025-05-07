using System;
using System.Collections.Generic;
using System.ComponentModel.DataAnnotations;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

using tiledpalettequant.Program;

namespace tiledpalettequant.Program
{
    /*
        Action[Action["StartQuantization"] = 0] = "StartQuantization";
    Action[Action["UpdateProgress"] = 1] = "UpdateProgress";
    Action[Action["UpdateQuantizedImage"] = 2] = "UpdateQuantizedImage";
    Action[Action["UpdatePalettes"] = 3] = "UpdatePalettes";
    Action[Action["DoneQuantization"] = 4] = "DoneQuantization";
    */

    public enum Dither
    {
        [Display(Name = "Off")]
        Off = 0,

        [Display(Name = "Fast")]
        Fast = 1,

        [Display(Name = "Slow")]
        Slow = 2
    }

    public enum DitherPattern
    {
        [Display(Name = "Diagonal4")]
        Diagonal4 = 0,

        [Display(Name = "Horizontal4")]
        Horizontal4 = 1,

        [Display(Name = "Vertical4")]
        Vertical4 = 2,

        [Display(Name = "Diagonal2")]
        Diagonal2 = 3,

        [Display(Name = "Horizontal2")]
        Horizontal2 = 4,

        [Display(Name = "Vertical2")]
        Vertical2 = 5
    }

    public enum ColorZeroBehavior
    {
        [Display(Name = "Unique")]
        Unique = 0,

        [Display(Name = "Shared")]
        Shared = 1,

        [Display(Name = "TransparentFromTransparent")]
        TransparentFromTransparent = 2,

        [Display(Name = "TransparentFromColor")]
        TransparentFromColor = 3
    }

    public enum DitherPixelType
    {
        /// <summary>
        /// The patterns used by the dither map are 1-fold symmetric. (See DitherMapSet)
        /// </summary>
        Type2,

        /// <summary>
        /// The patterns used by the dither map are 2-fold symmetric. (See DitherMapSet)
        /// </summary>
        Type4
    }
}
