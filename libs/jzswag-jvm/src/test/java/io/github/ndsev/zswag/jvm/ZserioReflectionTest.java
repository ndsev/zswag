package io.github.ndsev.zswag.jvm;

import org.junit.jupiter.api.Test;

import static org.assertj.core.api.Assertions.assertThat;
import static org.assertj.core.api.Assertions.assertThatThrownBy;

/**
 * Verifies that POJO getter reflection correctly resolves x-zserio-request-part
 * dotted paths, normalising snake_case to lowerCamel and unwrapping zserio
 * enum types via ZserioEnum.getGenericValue().
 */
class ZserioReflectionTest {

    @Test
    void wholeRequestSentinelReturnsRoot() {
        Outer obj = new Outer();
        assertThat(ZserioReflection.resolve(obj, "*")).isSameAs(obj);
        assertThat(ZserioReflection.resolve(obj, "")).isSameAs(obj);
    }

    @Test
    void singleSegmentResolvesGetter() {
        Outer obj = new Outer();
        obj.value = 42;
        assertThat(ZserioReflection.resolve(obj, "value")).isEqualTo(42);
    }

    @Test
    void dottedPathDescendsIntoNestedStructs() {
        Outer obj = new Outer();
        obj.inner = new Inner();
        obj.inner.label = "hello";
        assertThat(ZserioReflection.resolve(obj, "inner.label")).isEqualTo("hello");
    }

    @Test
    void snakeCaseSegmentIsNormalisedToLowerCamel() {
        Outer obj = new Outer();
        obj.enumValue = 7;  // getter is getEnumValue()
        assertThat(ZserioReflection.resolve(obj, "enum_value")).isEqualTo(7);
    }

    @Test
    void zserioEnumIsUnwrappedToGenericValue() {
        Outer obj = new Outer();
        obj.fakeEnum = FakeZserioEnum.SECOND;
        Object resolved = ZserioReflection.resolve(obj, "fakeEnum");
        assertThat(resolved).isInstanceOf(Number.class);
        assertThat(((Number) resolved).intValue()).isEqualTo(1);
    }

    @Test
    void missingGetterThrowsDescriptiveError() {
        Outer obj = new Outer();
        assertThatThrownBy(() -> ZserioReflection.resolve(obj, "nonExistentField"))
                .isInstanceOf(IllegalArgumentException.class)
                .hasMessageContaining("nonExistentField");
    }

    @Test
    void getterNameNormalisationConvertsSnakeCase() {
        assertThat(ZserioReflection.toGetterName("base", "get")).isEqualTo("getBase");
        assertThat(ZserioReflection.toGetterName("enum_value", "get")).isEqualTo("getEnumValue");
        assertThat(ZserioReflection.toGetterName("my_field_2", "get")).isEqualTo("getMyField2");
        assertThat(ZserioReflection.toGetterName("alreadyCamel", "get")).isEqualTo("getAlreadyCamel");
        assertThat(ZserioReflection.toGetterName("flag", "is")).isEqualTo("isFlag");
    }

    // -- Test fixtures matching zserio Java codegen conventions ---------

    public static class Outer {
        private int value;
        private Inner inner;
        private int enumValue;
        private FakeZserioEnum fakeEnum;

        public int getValue() { return value; }
        public Inner getInner() { return inner; }
        public int getEnumValue() { return enumValue; }
        public FakeZserioEnum getFakeEnum() { return fakeEnum; }
    }

    public static class Inner {
        private String label;
        public String getLabel() { return label; }
    }

    /** Mimics zserio-Java's generated enum: implements ZserioEnum with getValue/getGenericValue. */
    public enum FakeZserioEnum implements zserio.runtime.ZserioEnum, zserio.runtime.io.Writer, zserio.runtime.SizeOf {
        FIRST(0), SECOND(1), THIRD(2);

        private final int value;
        FakeZserioEnum(int v) { this.value = v; }
        public int getValue() { return value; }
        @Override public Number getGenericValue() { return value; }
        @Override public int bitSizeOf() { return 32; }
        @Override public int bitSizeOf(long bitPosition) { return 32; }
        @Override public long initializeOffsets() { return 32; }
        @Override public long initializeOffsets(long bitPosition) { return bitPosition + 32; }
        @Override public void write(zserio.runtime.io.BitStreamWriter out) {}
    }
}
