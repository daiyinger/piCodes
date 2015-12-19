import java.io.BufferedReader; import java.io.InputStreamReader; import 
java.io.PrintWriter; import java.net.ServerSocket; import 
java.net.Socket; import java.net.SocketException; import 
java.text.DecimalFormat; /**
 *
 */ public class SeverTest extends Thread {
	private Socket client;
	private static String split = "\t";
	private static String mac = "00000000";
	public SeverTest(Socket c) {
		this.client = c;
		try {
			client.setSoTimeout(1000 * 30);
		} catch (SocketException e) {
			e.printStackTrace();
		}
	}
	/**
	 * 线程方法
	 */
	public void run() {
		BufferedReader in = null;
		PrintWriter out = null;
		String strfromboss = null;
		String re = "";
		try {
			in = new BufferedReader(new 
InputStreamReader(client
					.getInputStream()));
			out = new PrintWriter(client.getOutputStream());
			strfromboss = in.readLine();
			strfromboss = strfromboss.trim();
			// 接收信息机数据
			System.out.println("[Client IP:" + 
client.getInetAddress() + "\n"
					+ "from client:" + strfromboss);
			// 拆分数据包
			String remap[] = strfromboss.split("\t");
			// 申请交易密码
			if (remap[1].equals("0100")) {
				re = type1(remap);
			}
	
			// 发送到信息机数据
			System.out.println("[Client IP:" + 
client.getInetAddress() + "\n"
					+ "to client:" + re);
			out.println(re);
			out.flush();
		} catch (Exception e) {
			System.out.println("错误" + e.toString());
		} finally {
			try {
				if (out != null)
					out.close();
				if (in != null)
					in.close();
				if (client != null)
					client.close();
			} catch (Exception ex) {
				System.out.println("错误" + 
ex.toString());
			}
		}
	}
	private static String type1(String[] arg) {
		return "helo world";
	}
	
	private static String makeln(int ln) {
		String strln = "";
		DecimalFormat df = new DecimalFormat("0000");
		strln = String.valueOf(df.format(ln));
		return strln;
	}
	/**
	 * 函数入口
	 *
	 * @param args
	 * @throws Exception
	 * @throws IOException
	 */
	public static void main(String[] args) throws Exception {
		ServerSocket server;
		server = new ServerSocket(8086);
		server.setReuseAddress(true);
		while (true) {
			System.out.println("wait for a client...");
			SeverTest ms = new SeverTest(server.accept());
			ms.start();
		}
	}
}
