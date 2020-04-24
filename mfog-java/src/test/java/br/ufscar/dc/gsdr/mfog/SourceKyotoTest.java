package br.ufscar.dc.gsdr.mfog;

import org.junit.jupiter.api.Test;

import static org.junit.jupiter.api.Assertions.*;

class SourceKyotoTest {

    @Test
    void transform() {
        String fromKyoto = "0.0,0.0,0.0,0.0,0.0,0.0,0.4,0.0,0.0,0.0,0.0,0.0,1,0,0,0,0,0,0,0,1,0,N";
        SourceKyoto.LabeledPoint labeledPoint = new SourceKyoto.LabeledPoint(new SourceKyoto.IdGenerator(), fromKyoto).invoke();
        assertEquals("N", labeledPoint.label, "Label should match");
        double[] fixed = {0.0,0.0,0.0,0.0,0.0,0.0,0.4,0.0,0.0,0.0,0.0,0.0,1,0,0,0,0,0,0,0,1,0};
        assertArrayEquals(fixed, labeledPoint.point.value, "point value should match");
        assertTrue(1.4697 > labeledPoint.point.fromOrigin(), "Distance should match gt");
        assertTrue(1.4696 < labeledPoint.point.fromOrigin(), "Distance should match lt");
    }
}