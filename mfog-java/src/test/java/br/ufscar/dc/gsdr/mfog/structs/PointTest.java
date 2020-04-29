package br.ufscar.dc.gsdr.mfog.structs;

import org.junit.jupiter.api.Test;
import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.ValueSource;

import static org.junit.jupiter.api.Assertions.*;

class PointTest {
    @Test
    void fromJson() {
        String jsonString = "{\"id\":0,\"time\":1587740835488,\"value\":[2.02E-5,0.023555555555555555,0.07827586206896552,0.011904761904761904,1,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,0,0]}";
        Point.fromJson(jsonString);
    }

    @Test
    void fromBytes() {
        Point[] ps = { Point.zero(22), Point.max(22) };
        for (Point p : ps) {
            fromBytes(p);
        }
    }
    void fromBytes(Point zero) {
        byte[] bytes = zero.toBytes();
        Point p2 = Point.fromBytes(bytes);
        pointEq(zero, p2);
    }

    public static void pointEq(Point a, Point b) {
        assertEquals(a.id, b.id, "Id should match");
        assertEquals(a.time, b.time, "Time should match");
        assertArrayEquals(a.value, b.value, "Value should match");
        assertEquals(a, b, "Point should match");
    }
}