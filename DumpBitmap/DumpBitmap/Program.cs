using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Linq;
using System.Text;

namespace DumpBitmap
{
    class Program
    {
        const string _inputFile = "img.bmp";
        const string _outputFile = "img.dump";

        static void Main(string[] args)
        {
            var image = Image.FromFile(_inputFile);
            var bytes = image.ToByteArray(ImageFormat.Bmp);
            WriteBytes(bytes);
        }

        private static void WriteBytes(byte[] input)
        {
            var bytes = input
                .Split(10)
                .Select(g => FormatByteString(g))
                .ToList();

            using (var wtr = new StreamWriter(_outputFile))
            {
                for (int i = 0; i < bytes.Count; i++)
                {
                    var line = bytes[i];
                    if (i == bytes.Count - 1)
                    {
                        line = line.Remove(line.Length - 1, 1);
                    }

                    wtr.WriteLine(line);
                }
            }
        }

        private static string FormatByteString(byte[] input)
        {
            var hex = new StringBuilder();
            foreach (var b in input)
            {
                hex.AppendFormat("0x{0:x2}, ", b);
            }

            hex.Remove(hex.Length - 1, 1);

            return hex.ToString();
        }
    }

    internal static class Extensions
    {
        public static List<byte[]> Split(this byte[] source, int size)
        {
            var result = new List<byte[]>();

            var next = new List<byte>();
            for (int i = 0; i < source.Length; i++)
            {
                next.Add(source[i]);
                if ((i + 1) % size == 0)
                {
                    result.Add(next.ToArray());
                    next = new List<byte>();
                }
            }

            if (next.Count > 0)
            {
                result.Add(next.ToArray());
            }

            return result;
        }

        public static byte[] ToByteArray(this Image source, ImageFormat format)
        {
            using (var stream = new MemoryStream())
            {
                source.Save(stream, format);
                return stream.ToArray();
            }
        }
    }
}
