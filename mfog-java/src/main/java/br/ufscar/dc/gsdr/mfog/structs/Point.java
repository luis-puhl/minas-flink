package br.ufscar.dc.gsdr.mfog.structs;

import org.apache.commons.lang3.SerializationUtils;
import org.json.*;

import java.io.InputStream;
import java.io.Serializable;
import java.util.Arrays;
import java.util.Objects;

public class Point implements Serializable {

    public static Point fromJson(String src) {
        return fromJson(new JSONObject(src));
    }
    public Point fromBytes(byte[] bytes) {
        return SerializationUtils.deserialize(bytes);
    }
    public Point fromBytes(InputStream stream) {
        return SerializationUtils.deserialize(stream);
    }

    public static Point fromJson(JSONObject src) {
        long id = src.getLong("id");
        JSONArray valueJson = src.getJSONArray("value");
        float[] value = new float[valueJson.length()];
        for (int i = 0; i < valueJson.length(); i++) {
            value[i] = valueJson.getFloat(i);
        }
        long time = src.getLong("time");
        return new Point(id, value, time);
    }
    public static Point zero(int dimension){
        float[] val = new float[dimension];
        return apply(0, val);
    }
    public static Point max(int dimension) {
        float[] val = new float[dimension];
        Arrays.fill(val, (float) 1.0);
        return apply(Long.MAX_VALUE, val);
    }
    public static String csv = "id,value,time";

    public static Point apply(long id, float[] value) {
        return apply(id, value, System.currentTimeMillis());
    }
    public static Point apply(long id, float[] value, long time) {
        return new Point(id, value, time);
    }

    public long id;
    public float[] value;
    public long time;

    public Point() {}
    public Point(long id, float[] value, long time) {
        this.id = id;
        this.value = value;
        this.time = time;
    }

    public JSONObject json() {
        return new JSONObject(this);
    }
    /*
    public String csv() {
        StringBuilder val = new StringBuilder();
        val.append("\"");
        for (float v : value) {
            val.append(v);
        }
        val.append("\"");
        return id + "," + val + "," + time;
    }
     */
    public int dimension() {
        return this.value.length;
    }
    // public float fromOrigin(implicit distanceOperator: Point.DistanceOperator): Float = this.distance(Point.zero(this.dimension))
    public double fromOrigin() {
        return this.distance(Point.zero(this.dimension()));
    }
    public double distance(Point other) {
        return this.euclideanDistance(other);
    }
    public Point plus(Point other) {
        checkSize(other);
        float[] val = new float[dimension()];
        for (int i = 0; i < value.length; i++) {
            val[i] = this.value[i] + other.value[i];
        }
        return apply(this.id, val);
    }

//        /**
//         * @param other
//         * @return Internal Product = a * b = [a_i * b_i, ...]
//         */
//        def *(other: Point): Float =
//                                                                 checkSize(other, this.value.zip(other.value).map(x => x._1 * x._2).sum)
//        def *(scalar: Float): Point =
//                                                                     Point(this.id, this.value.map(x => x * scalar))
//        def /(scalar: Float): Point =
//                                                                     this * (1/scalar)
        public Point scalarMul(float x) {
            float[] val = new float[dimension()];
            for (int i = 0; i < value.length; i++) {
                val[i] = this.value[i] * x;
            }
            return apply(this.id, val);
        }
        public Point neg() {
            return this.scalarMul(-1);
        }
        public Point minus(Point other) {
            checkSize(other);
            float[] val = new float[dimension()];
            for (int i = 0; i < value.length; i++) {
                val[i] = this.value[i] - other.value[i];
            }
            return apply(this.id, val);
        }

        /**
         * @return ∑(aᵢ²)
         * def unary_| : Float = this.value.map(x => Math.pow(x, 2)).sum
         */

//        /**
//         * @return ||point|| = √(|a|)
//         */
//        def unary_|| : Float =
//                                                     Math.sqrt(this.unary_|)
//
//        override def equals(other: Any): Boolean = other match {
//            case Point(id, value, time) => id == this.id && value.equals(this.value)
//            case _ => false
//        }
//
        public void checkSize(Point other) {
            if (this.dimension() != other.dimension()) {
                throw new RuntimeException("Mismatch dimensions. This is " + this.dimension() + " and other is " + other.dimension() + ".");
            }
        }

        public double euclideanDistance(Point other) {
            checkSize(other);
            float val = (float) 0.0;
            for (int i = 0; i < value.length; i++) {
                float diff = this.value[i] - other.value[i];
                val += diff * diff;
            }
            return Math.sqrt(val);
        }
//        def euclideanSqrDistance(other: Point): Float =
//                                                                                                        checkSize(other, (this - other).unary_|)
//        def taxiCabDistance(other: Point): Float =
//                                                                                             checkSize(other, (this - other).value.map(d => d.abs).sum)
//        def cosDistance(other: Point): Float =
//                                                                                     checkSize(other, (this * other) / (this.unary_|| * other.unary_||))
//        def distance(other: Point)(implicit distanceOperator: Point.DistanceOperator): Float =
//                                                                                                                                                                                     distanceOperator.compare(this, other)
//    }

    public long getId() {
        return id;
    }

    public void setId(long id) {
        this.id = id;
    }

    public float[] getValue() {
        return value;
    }

    public void setValue(float[] value) {
        this.value = value;
    }

    public long getTime() {
        return time;
    }

    public void setTime(long time) {
        this.time = time;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (!(o instanceof Point)) return false;
        Point point = (Point) o;
        return getId() == point.getId() &&
                                     getTime() == point.getTime() &&
                                     Arrays.equals(getValue(), point.getValue());
    }

    @Override
    public int hashCode() {
        int result = Objects.hash(getId(), getTime());
        result = 31 * result + Arrays.hashCode(getValue());
        return result;
    }

    @Override
    public String toString() {
        return "Point{id=" + id +", value=" + Arrays.toString(value) +", time=" + time + '}';
    }

    public byte[] toBytes() {
        return SerializationUtils.serialize(this);
    }
}

