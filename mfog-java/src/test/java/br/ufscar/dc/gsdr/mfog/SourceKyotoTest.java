package br.ufscar.dc.gsdr.mfog;

import br.ufscar.dc.gsdr.mfog.structs.LabeledExample;
import org.junit.jupiter.api.Test;

import static org.junit.jupiter.api.Assertions.*;

class SourceKyotoTest {

    @Test
    void transform() {
        String fromKyoto = "0.0,0.0,0.0,0.0,0.0,0.0,0.4,0.0,0.0,0.0,0.0,0.0,1,0,0,0,0,0,0,0,1,0,N";
        SourceKyoto.IdGenerator idGenerator = new SourceKyoto.IdGenerator();
        LabeledExample labeledPoint = LabeledExample.fromKyotoCSV(idGenerator.next(), fromKyoto);
        assertEquals("N", labeledPoint.label, "Label should match");
        float[] fixed = new float[]{0, 0, 0, 0, 0, 0, 0.4f, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0};
        assertArrayEquals(fixed, labeledPoint.point.value, "point value should match");
        assertTrue(1.4697 > labeledPoint.point.fromOrigin(), "Distance should match gt");
        assertTrue(1.4696 < labeledPoint.point.fromOrigin(), "Distance should match lt");
    }
}