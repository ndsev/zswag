package oauth_test;

/*!
OAuth2 Test Service - demonstrates OAuth 1.0 signature and HTTP Basic Auth
*/

struct DataRequest
{
    string clientName;
};

struct DataResponse
{
    string message;
    int32 secretValue;
};

/*!
Simple test service protected by OAuth2
*/
service OAuthTestService
{
    DataResponse getData(DataRequest);
};
