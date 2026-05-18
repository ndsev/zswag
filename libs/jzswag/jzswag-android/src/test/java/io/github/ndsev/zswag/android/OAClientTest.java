package io.github.ndsev.zswag.android;

import io.github.ndsev.zswag.api.HttpException;
import io.github.ndsev.zswag.shared.OpenAPIClient;
import org.junit.jupiter.api.Test;
import zserio.runtime.ZserioError;
import zserio.runtime.io.Writer;
import zserio.runtime.service.ServiceData;

import static org.assertj.core.api.Assertions.assertThat;
import static org.assertj.core.api.Assertions.assertThatThrownBy;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

/**
 * Unit tests for {@link OAClient} that don't require an Android Context
 * (uses the lower-level OpenAPIClient-delegate constructor). The
 * convenience constructors that take a Context are exercised by
 * instrumentation tests on a real device, which are out of this PR's scope.
 */
public class OAClientTest {

    @Test
    public void getOpenAPIClientReturnsUnderlyingDelegate() {
        OpenAPIClient delegate = mock(OpenAPIClient.class);
        OAClient zsw = new OAClient(delegate);
        assertThat(zsw.getOpenAPIClient()).isSameAs(delegate);
    }

    @Test
    @SuppressWarnings("unchecked")
    public void callMethodDelegatesToOpenAPIClient() throws Exception {
        OpenAPIClient delegate = mock(OpenAPIClient.class);
        when(delegate.callMethod(any(), any())).thenReturn("response".getBytes());
        OAClient zsw = new OAClient(delegate);

        Writer fakeWriter = mock(Writer.class);
        ServiceData<Writer> data = mock(ServiceData.class);
        when(data.getZserioObject()).thenReturn(fakeWriter);

        byte[] result = zsw.callMethod("powerMethod", data, null);
        assertThat(new String(result)).isEqualTo("response");
        verify(delegate).callMethod("powerMethod", fakeWriter);
    }

    @Test
    @SuppressWarnings("unchecked")
    public void callMethodThrowsZserioErrorWhenZserioObjectMissing() {
        OpenAPIClient delegate = mock(OpenAPIClient.class);
        OAClient zsw = new OAClient(delegate);
        ServiceData<Writer> data = mock(ServiceData.class);
        when(data.getZserioObject()).thenReturn(null);
        assertThatThrownBy(() -> zsw.callMethod("m", data, null))
                .isInstanceOf(ZserioError.class)
                .hasMessageContaining("getZserioObject() returned null");
    }

    @Test
    @SuppressWarnings("unchecked")
    public void callMethodWrapsHttpExceptionAsZserioError() throws Exception {
        OpenAPIClient delegate = mock(OpenAPIClient.class);
        when(delegate.callMethod(any(), any())).thenThrow(new HttpException("upstream-failed"));
        OAClient zsw = new OAClient(delegate);
        Writer fakeWriter = mock(Writer.class);
        ServiceData<Writer> data = mock(ServiceData.class);
        when(data.getZserioObject()).thenReturn(fakeWriter);
        assertThatThrownBy(() -> zsw.callMethod("powerMethod", data, null))
                .isInstanceOf(ZserioError.class)
                .hasMessageContaining("powerMethod failed")
                .hasMessageContaining("upstream-failed");
    }
}
