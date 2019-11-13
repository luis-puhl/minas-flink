package examples;

public class CourseCountPOJO {
    public String course;
    public String area;
    public Integer count;

    public CourseCountPOJO() {}

    public CourseCountPOJO(String course, String area, Integer count) {
        this.course = course;
        this.count = count;
        this.area = area;
    }

    @Override
    public String toString() {
        return course + "(" + area + "): " + count;
    }
}
