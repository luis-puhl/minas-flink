package examples;

import org.apache.flink.api.java.tuple.Tuple3;
import org.apache.flink.api.java.utils.ParameterTool;
import org.apache.flink.core.fs.FileSystem;
import org.apache.flink.streaming.api.datastream.DataStreamSource;
import org.apache.flink.streaming.api.environment.StreamExecutionEnvironment;
import org.apache.flink.streaming.api.functions.source.SourceFunction;

import java.math.BigDecimal;

public class Ex16CustomStonksSource {
    public static void main(String[] args) throws Exception {
        final StreamExecutionEnvironment env = StreamExecutionEnvironment.getExecutionEnvironment();
        final ParameterTool params = ParameterTool.fromArgs(args);
        env.getConfig().setGlobalJobParameters(params);

        DataStreamSource<Tuple3<String, Double, Double>> streamSource = env.addSource(new StockSource("BRL"));
        streamSource.print();
        streamSource.printToErr();

        streamSource.writeAsText("stonks", FileSystem.WriteMode.OVERWRITE);

        env.execute("Custom Stonks Source Example");
    }

    public static class StockSource implements SourceFunction<Tuple3<String, Double, Double>> {
        private String symbol;
        private int count;

        public StockSource(String symbol) {
            this.symbol = symbol;
            this.count = 0;
        }

        @Override
        public void run(SourceContext<Tuple3<String, Double, Double>> ctx) throws Exception {
            while (count < 10) {
                try {
                    YahooFinance.StockQuote quote = YahooFinance.get(symbol).getQuote();
                    BigDecimal price = quote.getPrice();
                    BigDecimal prevClose = quote.getPreviousClose();
                    ctx.collect(new Tuple3<>(symbol, price.doubleValue(), prevClose.doubleValue()));
                    count++;
                } catch (Exception e) {
                    System.err.println(e);
                }
            }
        }

        @Override
        public void cancel() {
        }
    }
}

class YahooFinance {
    public static class Stock {
        String symbol;

        public Stock(String symbol) {
            this.symbol = symbol;
        }

        public StockQuote getQuote() {
            Double price = Math.random() * 100;
            Double var = (Math.random() * 10) - 5;
            return new StockQuote(price, price + var);
        }
    }

    public static class StockQuote {
        BigDecimal price;
        BigDecimal previousClose;

        public StockQuote(Double price, Double previousClose) {
            this.price = BigDecimal.valueOf(price);
            this.previousClose = BigDecimal.valueOf(previousClose);
        }

        public BigDecimal getPrice() {
            return price;
        }

        public BigDecimal getPreviousClose() {
            return previousClose;
        }
    }

    public static Stock get(String symbol) {
        return new Stock(symbol);
    }
}