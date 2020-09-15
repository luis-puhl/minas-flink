package br.ufscar.dc.gsdr.mfog.structs;

import org.junit.jupiter.api.Test;

import static br.ufscar.dc.gsdr.mfog.structs.PointTest.pointEq;
import static org.junit.jupiter.api.Assertions.assertEquals;

public class LabeledExampleTest {
    @Test
    void fromBytes() {
        LabeledExample[] ps = {
                new LabeledExample("label", Point.zero(22)),
                new LabeledExample("lol", Point.max(22)),
                new LabeledExample("category", Point.zero(22)),
        };
        for (LabeledExample p : ps) {
            fromBytes(p);
        }
    }
    void fromBytes(LabeledExample zero) {
        byte[] bytes = zero.toBytes();
        LabeledExample p2 = LabeledExample.fromBytes(bytes);
        assertEquals(zero.point, p2.point, "Point should match");
        pointEq(zero.point, p2.point);
        assertEquals(zero.label, p2.label, "Label should match");
        assertEquals(zero, p2, "Point should match");
    }
}
