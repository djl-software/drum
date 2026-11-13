using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;

internal static class BridgeTests
{
    [DllImport("winsqlite3.dll",CallingConvention=CallingConvention.Cdecl)] private static extern int sqlite3_open_v2(byte[] path,out IntPtr db,int flags,IntPtr vfs);
    [DllImport("winsqlite3.dll",CallingConvention=CallingConvention.Cdecl)] private static extern int sqlite3_exec(IntPtr db,byte[] sql,IntPtr callback,IntPtr context,IntPtr error);
    [DllImport("winsqlite3.dll",CallingConvention=CallingConvention.Cdecl)] private static extern int sqlite3_close(IntPtr db);
    private static byte[] Utf8(string text) {return Encoding.UTF8.GetBytes(text+"\0");}
    private static void Check(bool ok,string message) {if(!ok)throw new Exception(message);}
    [STAThread] public static int Main()
    {
        var dir=Path.Combine(Path.GetTempPath(),"DrumBridgeTest-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(dir);
        try {
            LiveBrowserBridge.TestDropLifecycle(dir);
            var file=Path.Combine(dir,"effect.vst3");File.WriteAllText(file,"fixture");
            var dbPath=Path.Combine(dir,"test.db");IntPtr db;
            Check(sqlite3_open_v2(Utf8(dbPath),out db,6,IntPtr.Zero)==0,"Create fixture database");
            try {
                string sql="CREATE TABLE plugin_modules(module_id INTEGER,path TEXT);CREATE TABLE plugins(module_id INTEGER,name TEXT,vendor TEXT,dev_identifier TEXT,enabled INTEGER);"+
                    "INSERT INTO plugin_modules VALUES(1,'"+file.Replace("'","''")+"');"+
                    "INSERT INTO plugins VALUES(1,'Echo é','Maker','device:vst3:audiofx:one',1);"+
                    "INSERT INTO plugins VALUES(1,'Synth','Maker','device:vst3:instr:two',1);"+
                    "INSERT INTO plugins VALUES(1,'Old','Maker','device:vst:audiofx:three',1);"+
                    "INSERT INTO plugins VALUES(1,'Disabled','Maker','device:vst3:audiofx:four',0);"+
                    "INSERT INTO plugins VALUES(1,'Echo é','Other','device:vst3:audiofx:five',1);";
                Check(sqlite3_exec(db,Utf8(sql),IntPtr.Zero,IntPtr.Zero,IntPtr.Zero)==0,"Fixture schema");
            } finally {sqlite3_close(db);}
            var before=File.ReadAllBytes(dbPath);
            var rows=LiveBrowserBridge.Database.ResolveAt(dbPath,"Echo é");
            Check(rows.Count==2,"Ambiguous matches retained; Unicode name matched");
            Check((string)rows[0]["path"]==file,"Absolute module path retained");
            foreach(var name in new[]{"Synth","Old","Disabled","missing","' OR 1=1 --"})
                Check(LiveBrowserBridge.Database.ResolveAt(dbPath,name).Count==0,"Rejected "+name);
            File.Delete(file);Check(LiveBrowserBridge.Database.ResolveAt(dbPath,"Echo é").Count==0,"Missing module rejected");
            Check(Convert.ToBase64String(before)==Convert.ToBase64String(File.ReadAllBytes(dbPath)),"Live database remains unchanged");
            Console.WriteLine("Browser resolver: effects, ambiguity, Unicode, missing files, disabled plugins, SQL binding and read-only access passed.");return 0;
        } catch(Exception ex) {Console.Error.WriteLine(ex);return 1;}
        finally {Directory.Delete(dir,true);}
    }
}
