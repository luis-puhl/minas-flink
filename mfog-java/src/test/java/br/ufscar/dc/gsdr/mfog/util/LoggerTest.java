package br.ufscar.dc.gsdr.mfog.util;

import org.junit.jupiter.api.Test;

import static org.junit.jupiter.api.Assertions.*;

class LoggerTest {

    @Test
    void format() {
        Logger lg = Logger.getLogger("service");
        String format = lg.format("LVL", "message", 0);
        String expected = "1969-12-31T21:00:00.000 LVL   service message";
        assertEquals(expected, format, "Format error");
    }
}