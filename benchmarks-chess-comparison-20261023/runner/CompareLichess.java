package comparison;
import java.net.*;import java.nio.file.*;import java.util.*;import java.lang.reflect.*;
public final class CompareLichess {
 public static void main(String[] a) throws Exception {
  URLClassLoader oldL=new URLClassLoader(new URL[]{Path.of(a[0]).toUri().toURL()},ClassLoader.getPlatformClassLoader());
  URLClassLoader newL=new URLClassLoader(new URL[]{Path.of(a[1]).toUri().toURL()},ClassLoader.getPlatformClassLoader());
  Method oldEnc=Class.forName("org.lichess.compression.game.Encoder",true,oldL).getMethod("encode",String[].class);
  Method newEnc=Class.forName("org.lichess.compression.game.Encoder",true,newL).getMethod("encode",String[].class);
  long games=0,plies=0;
  for(int d=2;d<a.length;d++) for(String line:Files.readAllLines(Path.of(a[d]))) {
   String[] san=line.trim().isEmpty()?new String[0]:line.trim().split("\\s+");
   byte[] x=(byte[])oldEnc.invoke(null,(Object)san),y=(byte[])newEnc.invoke(null,(Object)san);
   if(!Arrays.equals(x,y))throw new IllegalStateException("byte mismatch dataset file="+a[d]+" game="+games);
   games++;plies+=san.length;
  }
  System.out.println("identical_games="+games+" plies="+plies);
 }
}
