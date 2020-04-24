package br.ufscar.dc.gsdr.mfog.structs;

import static org.junit.jupiter.api.Assertions.*;

class PointTest {

    @org.junit.jupiter.api.Test
    void fromJson() {
        String jsonString = "{\"id\":0,\"time\":1587740835488,\"value\":[2.02E-5,0.023555555555555555,0.07827586206896552,0.011904761904761904,1,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,0,0]}";
        Point.fromJson(jsonString);
    }

    @org.junit.jupiter.api.Test
    void testFromJson() {
    }

    @org.junit.jupiter.api.Test
    void json() {
    }
}