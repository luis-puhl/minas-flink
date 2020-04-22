package br.ufscar.dc.gsdr.mfog;

import static org.junit.jupiter.api.Assertions.*;

import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Tag;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.TestInfo;

import java.util.Arrays;

@Tag("fast")
class ClusterTest {

    @Test
    @DisplayName("Static csv model test")
    void fromMinasCsv() {
        String line = "0,N,normal,502,0,0.04553028494064095,0.1736759823342961,[2.888834262948207E-4, 0.020268260292164667, 0.04161011127902189, 0.020916334661354643, 1.0, 0.0, 0.0026693227091633474, 0.516593625498008, 0.5267529880478092, 1.9920318725099602E-4, 0.0, 7.968127490039841E-5, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0]";
        //
        String[] metaAndCenter = line.split(",\\[");
        assertEquals(2, metaAndCenter.length, "Split line should be 2");
        String meta = metaAndCenter[0];
        String centerString = metaAndCenter[1].replaceAll("[ \\]]", "");
        String[] split = meta.split(",");
        assertEquals(7, split.length, "Split header should be 7");
        String testCenter = "2.888834262948207E-4,0.020268260292164667,0.04161011127902189,0.020916334661354643,1.0,0.0,0.0026693227091633474,0.516593625498008,0.5267529880478092,1.9920318725099602E-4,0.0,7.968127490039841E-5,0.0,0.0,1.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0";
        assertEquals(testCenter, centerString, "Center before split");
        //
        System.out.println(Arrays.toString(split));
        for (int j = 0; j < split.length; j++) {
            assertNotNull(split[j], "Item " + j + " should not be null");
        }
        //
        long id                = Long.parseLong(split[0]);
        String label           = split[1];
        String category        = split[2];
        long matches           = Long.parseLong(split[3]);
        long time              = Long.parseLong(split[4]);
        double meanDistance    = Double.parseDouble(split[5]);
        double radius          = Double.parseDouble(split[6]);
        String[] centerStrings = centerString.split(",");
        //
        //[0, N, normal, 502, 0, 0.04553028494064095, 0.1736759823342961]
        assertEquals(0, id);
        assertEquals("N", label);
        assertEquals("normal", category);
        assertEquals(502, matches);
        assertEquals(0, time);
        assertEquals(0.04553028494064095, meanDistance);
        assertEquals(0.1736759823342961, radius);
        //
        double[] center        = new double[centerStrings.length];
        for (int i = 0; i < centerStrings.length; i++) {
            center[i] = Double.parseDouble(centerStrings[i]);
        }
        double[] expectedCenter = {2.888834262948207E-4, 0.020268260292164667, 0.04161011127902189, 0.020916334661354643, 1.0, 0.0, 0.0026693227091633474, 0.516593625498008, 0.5267529880478092, 1.9920318725099602E-4, 0.0, 7.968127490039841E-5, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0 };
        assertArrayEquals(expectedCenter, center);
        //
        assertEquals(22, centerStrings.length, "Split center should be 22. " + centerString);
        //
        Cluster c = Cluster.fromMinasCsv(line);
        assertEquals("N", c.label, "Expect cluster to have same class from csv");
    }
}