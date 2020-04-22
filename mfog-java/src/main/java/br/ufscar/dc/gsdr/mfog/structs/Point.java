package br.ufscar.dc.gsdr.mfog.structs;

import org.json.*;

import java.util.Arrays;

public class Point {
  public static Point fromJson(String src) {
    return fromJson(new JSONObject(src));
  }
  public static Point fromJson(JSONObject src) {
    long id = src.getLong("id");
    JSONArray valueJson = src.getJSONArray("value");
    double[] value = new double[valueJson.length()];
    for (int i = 0; i < valueJson.length(); i++) {
      value[i] = valueJson.getDouble(i);
    }
    long time = src.getLong("time");
    return new Point(id, value, time);
  }
  public static Point zero(int dimension){
    double[] val = new double[dimension];
    return apply(0, val);
  }
  public static Point max(int dimension) {
    double[] val = new double[dimension];
    Arrays.fill(val, 1.0);
    return apply(Long.MAX_VALUE, val);
  }
  public String csv = "id,value,time";

  public static Point apply(long id, double[] value) {
    return apply(id, value, System.currentTimeMillis());
  }
  public static Point apply(long id, double[] value, long time) {
    return new Point(id, value, time);
  }

  public long id;
  public double[] value;
  public long time;
  public Point(long id, double[] value, long time) {
    this.id = id;
    this.value = value;
    this.time = time;
  }

  public JSONObject json() {
    JSONObject obj = new JSONObject();
    obj.append("id", id);
    obj.append("value", new JSONArray(value));
    obj.append("time", time);
    return obj;
  }
  public String csv() {
    StringBuilder val = new StringBuilder();
    val.append("\"");
    for (double v : value) {
      val.append(v);
    }
    val.append("\"");
    return id + "," + val + "," + time;
  }
  public int dimension() {
    return this.value.length;
  }
  // public double fromOrigin(implicit distanceOperator: Point.DistanceOperator): Double = this.distance(Point.zero(this.dimension))
  public double fromOrigin() {
    return this.distance(Point.zero(this.dimension()));
  }
  public double distance(Point other) {
    return this.euclideanDistance(other);
  }
  public Point plus(Point other) {
    checkSize(other);
    double[] val = new double[dimension()];
    for (int i = 0; i < value.length; i++) {
      val[i] = this.value[i] + other.value[i];
    }
    return apply(this.id, val);
  }

//    /**
//     * @param other
//     * @return Internal Product = a * b = [a_i * b_i, ...]
//     */
//    def *(other: Point): Double =
//                                 checkSize(other, this.value.zip(other.value).map(x => x._1 * x._2).sum)
//    def *(scalar: Double): Point =
//                                   Point(this.id, this.value.map(x => x * scalar))
//    def /(scalar: Double): Point =
//                                   this * (1/scalar)
    public Point scalarMul(double x) {
      double[] val = new double[dimension()];
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
      double[] val = new double[dimension()];
      for (int i = 0; i < value.length; i++) {
        val[i] = this.value[i] - other.value[i];
      }
      return apply(this.id, val);
    }

    /**
     * @return ∑(aᵢ²)
     * def unary_| : Double = this.value.map(x => Math.pow(x, 2)).sum
     */

//    /**
//     * @return ||point|| = √(|a|)
//     */
//    def unary_|| : Double =
//                           Math.sqrt(this.unary_|)
//
//    override def equals(other: Any): Boolean = other match {
//      case Point(id, value, time) => id == this.id && value.equals(this.value)
//      case _ => false
//    }
//
    public void checkSize(Point other) {
      if (this.dimension() != other.dimension()) {
        throw new RuntimeException("Mismatch dimensions. This is " + this.dimension() + " and other is " + other.dimension() + ".");
      }
    }

    public Double euclideanDistance(Point other) {
      checkSize(other);
      double val = 0.0;
      for (int i = 0; i < value.length; i++) {
        double diff = this.value[i] - other.value[i];
        val += diff * diff;
      }
      return Math.sqrt(val);
    }
//    def euclideanSqrDistance(other: Point): Double =
//                                                    checkSize(other, (this - other).unary_|)
//    def taxiCabDistance(other: Point): Double =
//                                               checkSize(other, (this - other).value.map(d => d.abs).sum)
//    def cosDistance(other: Point): Double =
//                                           checkSize(other, (this * other) / (this.unary_|| * other.unary_||))
//    def distance(other: Point)(implicit distanceOperator: Point.DistanceOperator): Double =
//                                                                                           distanceOperator.compare(this, other)
//  }
}

