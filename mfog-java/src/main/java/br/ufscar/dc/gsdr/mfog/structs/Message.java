package br.ufscar.dc.gsdr.mfog.structs;

public class Message {
    public enum Intentions {
        SEND_RECEIVE,
        RECEIVE_ONLY,
        SEND_ONLY,
        DONE,
        CLASSIFIER,
        EVALUATOR,
    }

    public final Intentions value;

    public Message() {
        this(Intentions.SEND_RECEIVE);
    }
    public Message(Intentions value) {
        this.value = value;
    }

    @Override
    public String toString() {
        return "Message{" + "value=" + value + '}';
    }

    public boolean isReceive() {
        return value == Intentions.RECEIVE_ONLY || value == Intentions.SEND_RECEIVE;
    }
    public boolean isSend() {
        return value == Intentions.SEND_ONLY || value == Intentions.SEND_RECEIVE;
    }
    public boolean isDone() {
        return value == Intentions.DONE;
    }
    public boolean isClassifier() {
        return value == Intentions.CLASSIFIER;
    }
    public boolean isEvaluator() {
        return value == Intentions.EVALUATOR;
    }
}
