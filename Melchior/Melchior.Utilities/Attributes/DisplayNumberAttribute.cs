using System;

namespace Melchior.Utilities.Attributes
{
    /// <summary>
    /// DisplayAttribute is sealed (..?) This is a simple attribute class for adding an initializer for default values.
    /// </summary>
    public class DisplayNumberAttribute<T> : Attribute
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
        /// Used to define an (optional) minimum value for the display attribute. The constraint on 
        /// the template type (IComparable) is left off (on purpose); and it is assumed the developer
        /// knows when it is an appropriate use of the field.
        /// </summary>
        public T MinValue { get; set; }

        /// <summary>
        /// Used to define an (optional) maximum value for the display attribute. The constraint on 
        /// the template type (IComparable) is left off (on purpose); and it is assumed the developer
        /// knows when it is an appropriate use of the field.
        /// </summary>
        public T MaxValue { get; set; }

        /// <summary>
        /// Used to define an (optional) step value for the display attribute. The constraint on 
        /// the template type (IComparable) is left off (on purpose); and it is assumed the developer
        /// knows when it is an appropriate use of the field. The step value is used to define a constant
        /// difference used to modify the field or property.
        /// </summary>
        public T StepValue { get; set; }

        /// <summary>
        /// Optional command line flag used for this setting. This is useful for fields or properties linked to a command
        /// line.
        /// </summary>
        public string CommandLineFlag { get; set; }
    }
}
