package br.ufscar.dc.gsdr.mfog.structs;

import org.junit.jupiter.api.Test;

import static br.ufscar.dc.gsdr.mfog.structs.PointTest.pointEq;
import static org.junit.jupiter.api.Assertions.assertArrayEquals;
import static org.junit.jupiter.api.Assertions.assertEquals;

public class ClusterTest {
    @Test
    void fromBytes() {
        Cluster[] ps = {
                Cluster.apply(0, Point.zero(22), 0, "label"),
                Cluster.apply(1, Point.max(22), 0, "label", "category"),
                Cluster.apply(2, Point.max(22).scalarMul(0.5f), 1, "lol"),
        };
        for (Cluster p : ps) {
            fromBytes(p);
        }
    }
    void fromBytes(Cluster zero) {
        byte[] bytes = zero.toBytes();
        Cluster p2 = Cluster.fromBytes(bytes);
        assertEquals(zero.id, p2.id, "Id should match");
        assertEquals(zero.label, p2.label, "Label should match");
        assertEquals(zero.category, p2.category, "Category should match");
        assertEquals(zero.matches, p2.matches, "Matches should match");
        assertEquals(zero.center, p2.center, "Center should match");
        pointEq(zero.center, p2.center);
        assertEquals(zero.variance, p2.variance, "Variance should match");
        assertEquals(zero.time, p2.time, "Time should match");
        assertEquals(zero, p2, "Point should match");
    }
}
