using System;
using System.IO;
using System.Collections.Generic;
public class FindConst {
  public static int Main(string[] args) {
    string path = args.Length>0 ? args[0] : "full_image.bin";
    var f = File.OpenRead(path);
    long len = f.Length;
    byte[] buf = new byte[0x4000000];
    var targets = new Dictionary<string,ulong>{
      {"0x69CBDD6D2FCFDD7F",0x69CBDD6D2FCFDD7FUL},
      {"0x906933A7A6454651",0x906933A7A6454651UL},
      {"0x576A0C8EF709766F",0x576A0C8EF709766FUL},
      {"0xD99F6CAB2C376BF3",0xD99F6CAB2C376BF3UL},
      {"0x148158D8C7580ABD",0x148158D8C7580ABDUL},
      {"0x306AE931CF3763CB",0x306AE931CF3763CBUL},
      {"0x223ED5787CCC790D",0x223ED5787CCC790DUL},
    };
    Console.WriteLine("scanning "+path+" ("+len+" bytes)...");
    long pos=0; int hits=0;
    while (pos < len) {
      int n = (int)Math.Min(buf.Length, len-pos);
      f.Position=pos; f.Read(buf,0,n);
      for (long i=0;i+8<=n;i++){
        ulong v=BitConverter.ToUInt64(buf,(int)i);
        foreach(var kv in targets){
          if (kv.Value==v){
            // verify LE bytes
            Console.WriteLine("FOUND "+kv.Key+" @ RVA 0x"+(pos+i).ToString("x"));
            hits++;
          }
        }
      }
      pos += (long)(n-8);
    }
    Console.WriteLine("total hits: "+hits);
    return 0;
  }
}
