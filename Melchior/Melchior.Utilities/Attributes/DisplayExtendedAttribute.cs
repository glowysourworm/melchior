using System;

namespace Melchior.Utilities.Attributes
{
    /// <summary>
    /// DisplayAttribute is sealed (..?) This is a simple attribute class for adding an initializer for default values.
    /// </summary>
    public class DisplayExtendedAttribute<T> : Attribute
    {
        /// <summary>
        /// Display name for the property or field
        /// </summary>
        public string Name { get; set; }

        /// <summary>
        /// Display description of the property or field
        /// </summary>
        public string Description { get; set; }

        /// <summary>
        /// Used to define a default value for the property or field
        /// </summary>
        public T DefaultValue { get; set; }

        /// <summary>
        /// Optional command line flag used for this setting. This is useful for fields or properties linked to a command
        /// line.
        /// </summary>
        public string CommandLineFlag { get; set; }
    }
}
