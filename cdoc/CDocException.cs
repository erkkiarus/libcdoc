/*
 * libcdoc
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
#using System;

namespace OpenEid.CDoc
{
    /// <summary>
    /// Class representing exception occurred in CDoc library.
    /// </summary>
    public class CDocException : Exception
    {
        /// <summary>
        /// Error code.
        /// </summary>
        public int Code { get; }

        /// <summary>
        /// Constructs instance of the class from given error code and message.
        /// </summary>
        /// <param name="code">Error code</param>
        /// <param name="msg">Error message</param>
        public CDocException(int code, string msg) : base(msg)
        {
            Code = code;
        }
    }
}

