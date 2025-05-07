using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace tiledpalettequant.Program
{
    public class DitherMapSet
    {
        public Dictionary<DitherPattern, DitherMap> Maps { get; private set; }

        public DitherMapSet(DitherPixelType ditherType) 
        {
            this.Maps = new Dictionary<DitherPattern, DitherMap>();

            switch (ditherType)
            {
                case DitherPixelType.Type2:
                {
                    this.Maps.Add(DitherPattern.Diagonal2, new DitherMap(DitherPattern.Diagonal2));
                    this.Maps.Add(DitherPattern.Horizontal2, new DitherMap(DitherPattern.Horizontal2));
                    this.Maps.Add(DitherPattern.Vertical2, new DitherMap(DitherPattern.Vertical2));
                }
                break;
                case DitherPixelType.Type4:
                {
                    this.Maps.Add(DitherPattern.Diagonal4, new DitherMap(DitherPattern.Diagonal4));
                    this.Maps.Add(DitherPattern.Horizontal4, new DitherMap(DitherPattern.Horizontal4));
                }
                break;
                default:
                    throw new Exception("Unhandled DitherPixelType:  DitherMapSet.cs");
            }
        }
    }
}
