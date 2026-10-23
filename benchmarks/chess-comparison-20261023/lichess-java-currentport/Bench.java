package comparison;
import java.nio.file.*;
import java.util.*;
import org.lichess.compression.game.Encoder;
public final class Bench {
  public static void main(String[] args) throws Exception {
    System.out.println("dataset,games,plies,payload_bytes,bits_per_ply,encode_plies_s,decode_plies_s,encode_ms,decode_ms");
    for (String arg : args) {
      int eq=arg.indexOf("="); String label=arg.substring(0,eq);
      List<String[]> games=Files.readAllLines(Path.of(arg.substring(eq+1))).stream().map(s -> s.isBlank()?new String[0]:s.trim().split("\\s+")).toList();
      long plies=games.stream().mapToLong(g->g.length).sum();
      byte[][] warm=new byte[games.size()][]; for(int i=0;i<games.size();i++) warm[i]=Encoder.encode(games.get(i)); for(int i=0;i<games.size();i++) if(!Arrays.equals(Encoder.decode(warm[i],games.get(i).length).pgnMoves,games.get(i))) throw new IllegalStateException("warmup roundtrip "+label+" "+i);
      byte[][] packed=new byte[games.size()][]; long[] et=new long[5],dt=new long[5]; long bytes=0;
      for(int rep=0;rep<5;rep++){
        long t=System.nanoTime(); for(int i=0;i<games.size();i++){packed[i]=Encoder.encode(games.get(i));if(packed[i]==null)throw new IllegalStateException("encoder rejected "+label+" "+i);} et[rep]=System.nanoTime()-t;
        if(rep==0)for(byte[] b:packed)bytes+=b.length;
        t=System.nanoTime();for(int i=0;i<games.size();i++){if(!Arrays.equals(Encoder.decode(packed[i],games.get(i).length).pgnMoves,games.get(i)))throw new IllegalStateException("roundtrip "+label+" "+i);}dt[rep]=System.nanoTime()-t;
      }
      double[] es=new double[5],ds=new double[5];for(int i=0;i<5;i++){es[i]=plies/(et[i]/1e9);ds[i]=plies/(dt[i]/1e9);}Arrays.sort(es);Arrays.sort(ds);
      System.out.printf(Locale.ROOT,"%s,%d,%d,%d,%.6f,%.2f,%.2f,%s,%s%n",label,games.size(),plies,bytes,8.0*bytes/plies,es[2],ds[2],fmt(et),fmt(dt));
    }
  }
  static String fmt(long[] a){StringJoiner j=new StringJoiner(";");for(long n:a)j.add(String.format(Locale.ROOT,"%.2f",n/1e6));return j.toString();}
}
