package br.ufscar.dc.gsdr.mfog.structs;

import com.esotericsoftware.kryo.io.Input;
import com.esotericsoftware.kryo.io.Output;
import org.apache.commons.lang3.SerializationUtils;
import org.json.JSONObject;

import java.io.*;
import java.util.Objects;

public class LabeledExample implements Serializable, WithSerializable<LabeledExample> {

    public Point point;
    public String label;

    public LabeledExample() {
    }

    public LabeledExample(String label, Point point) {
        this.point = point;
        this.label = label;
    }

    public static LabeledExample fromJson(String src) {
        return fromJson(new JSONObject(src));
    }

    public static LabeledExample fromJson(JSONObject src) {
        LabeledExample l = new LabeledExample();
        l.label = src.getString("label");
        l.point = Point.fromJson(src.getJSONObject("point"));
        return l;
    }

    public static LabeledExample fromBytes(byte[] bytes) {
        return SerializationUtils.deserialize(bytes);
    }

    public static LabeledExample fromBytes(InputStream stream) {
        return SerializationUtils.deserialize(stream);
    }

    static public LabeledExample fromKyotoCSV(int id, String line) {
        // 0.0,0.0,0.0,0.0,0.0,0.0,0.4,0.0,0.0,0.0,0.0,0.0,1,0,0,0,0,0,0,0,1,0,N
        String[] lineSplit = line.split(",");
        float[] floats = new float[lineSplit.length - 1];
        for (int j = 0; j < floats.length - 1; j++) {
            floats[j] = Float.parseFloat(lineSplit[j]);
        }
        String label = lineSplit[lineSplit.length - 1];
        Point point = Point.apply(id, floats);
        return new LabeledExample(label, point);
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (!(o instanceof LabeledExample)) return false;
        LabeledExample that = (LabeledExample) o;
        return point.equals(that.point) && label.equals(that.label);
    }

    @Override
    public int hashCode() {
        return Objects.hash(point, label);
    }

    public Point getPoint() {
        return point;
    }

    public void setPoint(Point point) {
        this.point = point;
    }

    public String getLabel() {
        return label;
    }

    public void setLabel(String label) {
        this.label = label;
    }

    @Override
    public String toString() {
        return "LabeledExample{point=" + point + ", label='" + label + "'}";
    }

    public JSONObject json() {
        return new JSONObject(this);
    }

    public byte[] toBytes() {
        return SerializationUtils.serialize(this);
    }

    @Override
    public LabeledExample reuseFromDataInputStream(DataInputStream in) throws IOException {
        this.label = in.readUTF();
        if (this.point == null) {
            this.point = new Point();
        }
        this.point = this.point.reuseFromDataInputStream(in);
        return this;
    }

    public LabeledExample reuseFromDataInputStream(Input in) {
        this.label = in.readString();
        if (this.point == null) {
            this.point = new Point();
        }
        this.point = this.point.reuseFromDataInputStream(in);
        return this;
    }

    @Override
    public void toDataOutputStream(DataOutputStream out) throws IOException {
        out.writeUTF(label);
        point.toDataOutputStream(out);
    }

    public void toDataOutputStream(Output out) {
        out.writeString(label);
        point.toDataOutputStream(out);
    }

}