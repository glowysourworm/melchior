using System;

namespace tiledpalettequant.Program
{
    public class DitherMap
    {
        public readonly int[,] Map = new int[2, 2];

        public DitherMap(DitherPattern pattern) 
        {
            switch (pattern)
            {
                case DitherPattern.Diagonal4:
                {
                    this.Map = new int[2, 2]
                    {
                        { 0, 2 },
                        { 3, 1 },
                    };
                }
                break;
                case DitherPattern.Horizontal4:
                {
                    this.Map = new int[2, 2]
                    {
                        { 0, 3 },
                        { 1, 2 },
                    };
                }
                break;
                case DitherPattern.Vertical4:
                {
                    this.Map = new int[2, 2]
                    {
                        { 0, 1 },
                        { 3, 2 },
                    };
                }
                break;
                case DitherPattern.Diagonal2:
                {
                    this.Map = new int[2, 2]
                    {
                        { 0, 1 },
                        { 1, 0 },
                    };
                }
                break;
                case DitherPattern.Horizontal2:
                {
                    this.Map = new int[2, 2]
                    {
                        { 0, 1 },
                        { 0, 1 },
                    };
                }
                break;
                case DitherPattern.Vertical2:
                {
                    this.Map = new int[2, 2]
                    {
                        { 0, 0 },
                        { 1, 1 },
                    };
                }
                break;
                default:
                    throw new Exception("Unhandled DitherPattern:  DitherMap.cs");
            }
        }
    }
}
