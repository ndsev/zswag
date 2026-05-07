package io.github.ndsev.zswag.shared;

import io.github.ndsev.zswag.api.HttpException;
import io.github.ndsev.zswag.api.IOpenAPIClient;
import org.junit.jupiter.api.Test;

import java.util.Map;

import static org.assertj.core.api.Assertions.assertThat;
import static org.assertj.core.api.Assertions.assertThatThrownBy;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

class ZswagServiceClientTest {

    /** Sample reflection context with parameter-style getters. */
    public static final class CalculatorContext {
        public Integer getX() { return 5; }
        public String getName() { return "n"; }
        public Boolean getFlag() { return null; }     // null values are skipped
        @SuppressWarnings("unused")
        public Object getRaw(int param) { return null; }  // ignored: takes parameter
        public Object getNothing() { return null; }   // ignored: returns null
    }

    /** A context whose getter throws to exercise the ReflectiveOperationException branch. */
    public static final class FailingContext {
        public String getBoom() {
            throw new IllegalStateException("simulated reflection failure");
        }
    }

    @Test
    void callMethodBuildsPathFromServiceIdentifierAndDelegates() throws HttpException {
        IOpenAPIClient mockClient = mock(IOpenAPIClient.class);
        when(mockClient.callMethod(any(), any(), any())).thenReturn("response".getBytes());
        ZswagServiceClient svc = new ZswagServiceClient("calc.Calculator", mockClient);
        byte[] resp = svc.callMethod("powerMethod", "request".getBytes(), new CalculatorContext());
        assertThat(new String(resp)).isEqualTo("response");
        verify(mockClient).callMethod(eq("/calc/Calculator/powerMethod"), any(), any());
    }

    @Test
    void callMethodConvertsNullResponseToEmptyArray() throws HttpException {
        IOpenAPIClient mockClient = mock(IOpenAPIClient.class);
        when(mockClient.callMethod(any(), any(), any())).thenReturn(null);
        ZswagServiceClient svc = new ZswagServiceClient("svc", mockClient);
        byte[] resp = svc.callMethod("m", new byte[0], new Object());
        assertThat(resp).isEmpty();
    }

    @Test
    void callMethodPropagatesHttpException() throws HttpException {
        IOpenAPIClient mockClient = mock(IOpenAPIClient.class);
        when(mockClient.callMethod(any(), any(), any())).thenThrow(new HttpException("boom"));
        ZswagServiceClient svc = new ZswagServiceClient("s", mockClient);
        assertThatThrownBy(() -> svc.callMethod("m", new byte[0], new Object()))
                .isInstanceOf(HttpException.class)
                .hasMessageContaining("boom");
    }

    @Test
    void getOpenAPIClientReturnsUnderlyingClient() {
        IOpenAPIClient mockClient = mock(IOpenAPIClient.class);
        ZswagServiceClient svc = new ZswagServiceClient("s", mockClient);
        assertThat(svc.getOpenAPIClient()).isSameAs(mockClient);
        assertThat(svc.getServiceIdentifier()).isEqualTo("s");
    }

    @Test
    @SuppressWarnings("unchecked")
    void parameterMapExtractedFromGettersInContext() throws Exception {
        IOpenAPIClient mockClient = mock(IOpenAPIClient.class);
        when(mockClient.callMethod(any(), any(), any())).thenReturn(new byte[0]);
        ZswagServiceClient svc = new ZswagServiceClient("s", mockClient);
        svc.callMethod("m", new byte[0], new CalculatorContext());

        // Capture the parameter map passed to the underlying client.
        org.mockito.ArgumentCaptor<Map<String, Object>> captor = org.mockito.ArgumentCaptor.forClass(Map.class);
        verify(mockClient).callMethod(any(), captor.capture(), any());
        Map<String, Object> params = captor.getValue();
        assertThat(params).containsEntry("x", 5).containsEntry("name", "n");
        assertThat(params).doesNotContainKey("flag");      // null is skipped
        assertThat(params).doesNotContainKey("nothing");   // null is skipped
        assertThat(params).doesNotContainKey("class");     // Object.class.getClass() is skipped
    }

    @Test
    void reflectionFailureSurfacesAsHttpException() {
        IOpenAPIClient mockClient = mock(IOpenAPIClient.class);
        ZswagServiceClient svc = new ZswagServiceClient("s", mockClient);
        // FailingContext.getBoom() throws → wrapping reflective invocation surfaces InvocationTargetException
        // which is a ReflectiveOperationException → mapped to HttpException.
        assertThatThrownBy(() -> svc.callMethod("m", new byte[0], new FailingContext()))
                .isInstanceOf(HttpException.class)
                .hasMessageContaining("boom");
    }
}
