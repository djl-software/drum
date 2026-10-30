// Windows Live-browser drag adapter. No input injection, network or Set changes.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Web.Script.Serialization;
using System.Windows.Automation;
using System.Windows.Forms;

internal sealed class LiveBrowserBridge : Form
{
    private delegate IntPtr MouseProc(int code, IntPtr message, IntPtr data);
    [StructLayout(LayoutKind.Sequential)] private struct PointNative { public int X, Y; }
    [StructLayout(LayoutKind.Sequential)] private struct MouseData { public PointNative Pt; public uint Data, Flags, Time; public UIntPtr Extra; }
    [DllImport("user32.dll")] private static extern IntPtr SetWindowsHookEx(int id, MouseProc callback, IntPtr module, uint thread);
    [DllImport("user32.dll")] private static extern bool UnhookWindowsHookEx(IntPtr hook);
    [DllImport("user32.dll")] private static extern IntPtr CallNextHookEx(IntPtr hook, int code, IntPtr message, IntPtr data);
    [DllImport("user32.dll")] private static extern IntPtr WindowFromPoint(PointNative point);
    [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint pid);
    [DllImport("user32.dll")] private static extern short GetAsyncKeyState(int key);
    [DllImport("user32.dll")] private static extern bool GetCursorPos(out PointNative point);
    [DllImport("user32.dll")] private static extern bool IsWindow(IntPtr hwnd);
    [DllImport("user32.dll")] private static extern bool ShowWindow(IntPtr hwnd,int command);
    [DllImport("user32.dll")] private static extern IntPtr SetParent(IntPtr child, IntPtr parent);
    [DllImport("user32.dll")] private static extern bool SetWindowPos(IntPtr hwnd, IntPtr after, int x, int y, int w, int h, uint flags);
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode)] private static extern IntPtr GetModuleHandle(string name);
    private readonly int hostPid;
    private readonly IntPtr parent;
    private readonly string folder;
    private readonly MouseProc callback;
    private readonly Label label = new Label();
    private readonly System.Windows.Forms.Timer timer = new System.Windows.Forms.Timer();
    private readonly JavaScriptSerializer json = new JavaScriptSerializer();
    private IntPtr hook;
    private int generation, resolving;
    private string candidate = "", context = "";
    private DateTime captured, lastHeld;
    private List<Dictionary<string, object>> matches;
    private bool enabled, dropping;
    private int x,y,w,h;
    private PointNative origin;

    [STAThread] private static void Main(string[] args)
    {
        if(args.Length==1 && args[0]=="--self-test") { Database.SelfTest(); return; }
        if(args.Length!=3)return;
        Application.EnableVisualStyles();
        using(var bridge=new LiveBrowserBridge(int.Parse(args[0]),new IntPtr(long.Parse(args[1])),args[2]))
        { Application.Run(); }
    }
    private LiveBrowserBridge(int pid,IntPtr hwnd,string output)
    {
        hostPid=pid;parent=hwnd;folder=output;
        FormBorderStyle=FormBorderStyle.None;TopLevel=false;ShowInTaskbar=false;AllowDrop=true;
        BackColor=Color.FromArgb(25,43,52);ForeColor=Color.FromArgb(103,223,197);
        label.Dock=DockStyle.Fill;label.TextAlign=ContentAlignment.MiddleCenter;label.Font=new Font("Segoe UI",12);
        Controls.Add(label);SetParent(Handle,parent);Show();ShowWindow(Handle,0);
        callback=Observe;hook=SetWindowsHookEx(14,callback,GetModuleHandle(null),0);
        DragEnter+=OnDrag;DragOver+=OnDrag;
        DragDrop+=delegate(object sender,DragEventArgs e) {
            if(!Fresh())return;
            dropping=true;
            var result=new Dictionary<string,object>{{"context",context},{"name",candidate},{"matches",matches}};
            var temp=Path.Combine(folder,"pending.tmp");
            try {File.WriteAllText(temp,json.Serialize(result));File.Move(temp,Path.Combine(folder,"drop-"+Guid.NewGuid().ToString("N")+".json"));} catch(IOException) {}
            candidate="";ShowWindow(Handle,0);dropping=false;
        };
        timer.Interval=40;timer.Tick+=Tick;timer.Start();
    }
    private bool Fresh() { return enabled && candidate.Length>0 && matches!=null && matches.Count>0 && (DateTime.UtcNow-captured).TotalSeconds<20; }
    private void OnDrag(object sender,DragEventArgs e) { e.Effect=Fresh()?DragDropEffects.Copy:DragDropEffects.None; }
    private void Tick(object sender,EventArgs e)
    {
        if(!IsWindow(parent) || File.Exists(Path.Combine(folder,"stop"))) {Application.ExitThread();return;}
        try {
            var request=json.Deserialize<Dictionary<string,object>>(File.ReadAllText(Path.Combine(folder,"request.json")));
            string next=(string)request["context"];
            if(next!=context){candidate="";++generation;}
            context=next;enabled=(bool)request["enabled"];
            x=Convert.ToInt32(request["x"]);y=Convert.ToInt32(request["y"]);w=Convert.ToInt32(request["width"]);h=Convert.ToInt32(request["height"]);
        } catch {enabled=false;}
        bool held=(GetAsyncKeyState(1)&0x8000)!=0;
        PointNative cursor;GetCursorPos(out cursor);
        bool moved=Math.Abs(cursor.X-origin.X)+Math.Abs(cursor.Y-origin.Y)>6;
        if(held)lastHeld=DateTime.UtcNow;
        if(Fresh() && moved && (held || (DateTime.UtcNow-lastHeld).TotalMilliseconds<300) && w>0 && h>0) {
            label.Text="Release to add " + candidate + "\nVST3 instrument effect chain";
            SetWindowPos(Handle,IntPtr.Zero,x,y,w,h,0x0010|0x0040); // no activate, show child
        } else if(!dropping) ShowWindow(Handle,0);
    }
    private IntPtr Observe(int code,IntPtr message,IntPtr data)
    {
        if(code>=0 && message.ToInt64()==0x201) {
            int current=++generation;candidate="";
            var point=((MouseData)Marshal.PtrToStructure(data,typeof(MouseData))).Pt;origin=point;
            uint pid;GetWindowThreadProcessId(WindowFromPoint(point),out pid);
            if(enabled && pid==(uint)hostPid && Interlocked.CompareExchange(ref resolving,1,0)==0)
                ThreadPool.QueueUserWorkItem(delegate {try {Identify(point,current);} finally {Interlocked.Exchange(ref resolving,0);}});
        }
        return CallNextHookEx(hook,code,message,data);
    }
    private void Identify(PointNative point,int current)
    {
        string name="";bool browser=false;
        List<Dictionary<string,object>> found=null;
        try {
            var element=AutomationElement.FromPoint(new System.Windows.Point(point.X,point.Y));
            for(int depth=0;element!=null && depth<24;depth++) {
                var info=element.Current;
                if(info.ProcessId!=hostPid)return;
                if(name.Length==0 && (info.ControlType==ControlType.TreeItem || info.ControlType==ControlType.DataItem || info.ControlType==ControlType.ListItem))name=info.Name;
                if(info.AutomationId=="ContentBrowser") {browser=true;break;}
                element=TreeWalker.ControlViewWalker.GetParent(element);
            }
            if(browser && name.Length>0)found=Database.Resolve(name);
        } catch {return;}
        if(found==null || found.Count==0 || IsDisposed)return;
        try {BeginInvoke((Action)delegate {if(current==generation && enabled) {candidate=name;matches=found;captured=DateTime.UtcNow;}});} catch(InvalidOperationException) {}
    }
    protected override void Dispose(bool disposing)
    {
        if(disposing) {timer.Dispose();if(hook!=IntPtr.Zero) {UnhookWindowsHookEx(hook);hook=IntPtr.Zero;}}
        base.Dispose(disposing);
    }

    internal static class Database
    {
        [DllImport("winsqlite3.dll",CallingConvention=CallingConvention.Cdecl)] private static extern int sqlite3_open_v2(byte[] name,out IntPtr db,int flags,IntPtr vfs);
        [DllImport("winsqlite3.dll",CallingConvention=CallingConvention.Cdecl)] private static extern int sqlite3_close(IntPtr db);
        [DllImport("winsqlite3.dll",CallingConvention=CallingConvention.Cdecl)] private static extern int sqlite3_prepare_v2(IntPtr db,byte[] sql,int length,out IntPtr stmt,IntPtr tail);
        [DllImport("winsqlite3.dll",CallingConvention=CallingConvention.Cdecl)] private static extern int sqlite3_bind_text(IntPtr stmt,int column,byte[] text,int length,IntPtr destructor);
        [DllImport("winsqlite3.dll",CallingConvention=CallingConvention.Cdecl)] private static extern int sqlite3_step(IntPtr stmt);
        [DllImport("winsqlite3.dll",CallingConvention=CallingConvention.Cdecl)] private static extern IntPtr sqlite3_column_text(IntPtr stmt,int column);
        [DllImport("winsqlite3.dll",CallingConvention=CallingConvention.Cdecl)] private static extern int sqlite3_finalize(IntPtr stmt);
        private static byte[] Utf8(string text) {return Encoding.UTF8.GetBytes(text+"\0");}
        private static string Text(IntPtr stmt,int column) {
            var p=sqlite3_column_text(stmt,column);if(p==IntPtr.Zero)return "";
            int n=0;while(Marshal.ReadByte(p,n)!=0 && n<32768)n++;
            var b=new byte[n];Marshal.Copy(p,b,0,n);return Encoding.UTF8.GetString(b);
        }
        internal static List<Dictionary<string,object>> Resolve(string name)
        {
            return ResolveAt(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"Ableton","Live Database","Live-plugins-1.db"),name);
        }
        internal static List<Dictionary<string,object>> ResolveAt(string path,string name)
        {
            var rows=new List<Dictionary<string,object>>();IntPtr db=IntPtr.Zero,stmt=IntPtr.Zero;
            try {
                if(sqlite3_open_v2(Utf8(path),out db,1,IntPtr.Zero)!=0)return rows; // read only
                string sql="SELECT DISTINCT p.name,p.vendor,p.dev_identifier,m.path FROM plugins p JOIN plugin_modules m USING(module_id) WHERE p.name=?1 COLLATE NOCASE AND p.dev_identifier LIKE 'device:vst3:audiofx:%' AND p.enabled=1";
                if(sqlite3_prepare_v2(db,Utf8(sql),-1,out stmt,IntPtr.Zero)!=0)return rows;
                var text=Utf8(name);sqlite3_bind_text(stmt,1,text,text.Length-1,new IntPtr(-1));
                while(sqlite3_step(stmt)==100 && rows.Count<32) {
                    var file=Text(stmt,3);if(!file.EndsWith(".vst3",StringComparison.OrdinalIgnoreCase) || !Path.IsPathRooted(file) || (!File.Exists(file) && !Directory.Exists(file)))continue;
                    rows.Add(new Dictionary<string,object>{{"name",Text(stmt,0)},{"vendor",Text(stmt,1)},{"id",Text(stmt,2)},{"path",file}});
                }
            } finally {if(stmt!=IntPtr.Zero)sqlite3_finalize(stmt);if(db!=IntPtr.Zero)sqlite3_close(db);}
            return rows;
        }
        internal static void SelfTest()
        {
            if(Resolve("a' OR 1=1 --").Count!=0)throw new Exception("Name must be a bound value");
            if(Resolve("DJL Drum").Count!=0)throw new Exception("Instrument must be excluded");
            foreach(var row in Resolve("Pro-C 2"))if(!((string)row["id"]).StartsWith("device:vst3:audiofx:"))throw new Exception("Non-effect matched");
        }
    }
}
