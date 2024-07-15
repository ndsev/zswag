#include "oaclient.hpp"

#include <cassert>
#include "stx/format.h"
#include "zserio/ITypeInfo.h"

namespace zswagcl
{

OAClient::OAClient(zswagcl::OpenAPIConfig config,
                   std::unique_ptr<httpcl::IHttpClient> client,
                   httpcl::Config httpConfig,
                   uint32_t serverIndex)
    : client_(std::move(config), std::move(httpConfig), std::move(client), serverIndex)
{}

template<typename arr_elem_t>
ParameterValue reflectableArrayToParameterValue(std::function<void(std::vector<arr_elem_t>&, size_t)> appendFun, size_t length, ParameterValueHelper& helper) {
    std::vector<arr_elem_t> values;
    values.reserve(length);
    for (auto i = 0; i < length; ++i) {
        appendFun(values, i);
    }
    return helper.array(values);
}

ParameterValue reflectableToParameterValue(std::string const& fieldName, zserio::IReflectableConstPtr const& ref, zserio::ITypeInfo const& refType, ParameterValueHelper& helper)
{
    switch (refType.getCppType())
    {
        case zserio::CppType::BOOL:
            if (ref->isArray()) {
                return reflectableArrayToParameterValue<uint8_t>([&](auto& arr, auto i) {
                    arr.emplace_back(static_cast<uint8_t>(ref->at(i)->getBool()));
                }, ref->size(), helper);
            }
            return helper.value(static_cast<uint8_t>(ref->getBool()));
        case zserio::CppType::INT8:
        case zserio::CppType::INT16:
        case zserio::CppType::INT32:
        case zserio::CppType::INT64:
            if (ref->isArray()) {
                return reflectableArrayToParameterValue<int64_t>([&](auto& arr, auto i) {
                    arr.emplace_back(ref->at(i)->toInt());
                }, ref->size(), helper);
            }
            return helper.value(ref->toInt());
        case zserio::CppType::UINT8:
        case zserio::CppType::UINT16:
        case zserio::CppType::UINT32:
        case zserio::CppType::UINT64:
            if (ref->isArray()) {
                return reflectableArrayToParameterValue<uint64_t>([&](auto& arr, auto i) {
                    arr.emplace_back(ref->at(i)->toUInt());
                }, ref->size(), helper);
            }
            return helper.value(ref->toUInt());
        case zserio::CppType::FLOAT:
        case zserio::CppType::DOUBLE:
            if (ref->isArray()) {
                return reflectableArrayToParameterValue<double>([&](auto& arr, auto i) {
                    arr.emplace_back(ref->at(i)->toDouble());
                }, ref->size(), helper);
            }
            return helper.value(ref->toDouble());
        case zserio::CppType::STRING:
            if (ref->isArray()) {
                return reflectableArrayToParameterValue<std::string>([&](auto& arr, auto i) {
                    arr.emplace_back(ref->at(i)->toString());
                }, ref->size(), helper);
            }
            return helper.value(ref->toString());
        case zserio::CppType::BIT_BUFFER: {
            if (ref->isArray()) {
                return reflectableArrayToParameterValue<std::string>([&](auto& arr, auto i) {
                    auto const& buffer = ref->at(i)->getBytes();
                    arr.emplace_back(buffer.begin(), buffer.end());
                }, ref->size(), helper);
            }
            auto const& buffer = ref->getBytes();
            return helper.binary(std::vector<uint8_t>(buffer.begin(), buffer.end()));
        }
        case zserio::CppType::BYTES: {
            if (ref->isArray()) {
                return reflectableArrayToParameterValue<std::string>([&](auto& arr, auto i) {
                    auto const& buffer = ref->at(i)->getBitBuffer();
                    arr.emplace_back(buffer.getBuffer(), buffer.getBuffer() + buffer.getByteSize());
                }, ref->size(), helper);
            }
            auto const& buffer = ref->getBitBuffer();
            return helper.binary(std::vector<uint8_t>(buffer.getBuffer(), buffer.getBuffer() + buffer.getByteSize()));
        }
        case zserio::CppType::ENUM:
        case zserio::CppType::BITMASK: {
            return reflectableToParameterValue(fieldName, ref, refType.getUnderlyingType(), helper);
        }
        case zserio::CppType::STRUCT:
        case zserio::CppType::CHOICE:
        case zserio::CppType::UNION: {
            if (ref->isArray()) {
                return reflectableArrayToParameterValue<std::string>([&](auto& arr, auto i) {
                    zserio::BitBuffer buffer(ref->at(i)->bitSizeOf());
                    zserio::BitStreamWriter writer(buffer);
                    ref->at(i)->write(writer);
                    arr.emplace_back(buffer.getBuffer(), buffer.getBuffer() + buffer.getByteSize());
                }, ref->size(), helper);
            }
            zserio::BitBuffer buffer(ref->bitSizeOf());
            zserio::BitStreamWriter writer(buffer);
            ref->write(writer);
            return helper.binary(std::vector<uint8_t>(buffer.getBuffer(), buffer.getBuffer() + buffer.getByteSize()));
        }

        case zserio::CppType::SQL_TABLE:
        case zserio::CppType::SQL_DATABASE:
        case zserio::CppType::SERVICE:
        case zserio::CppType::PUBSUB:
            break;
    }

    throw std::runtime_error(stx::format("Failed to serialize field '{}' for HTTP transport.", fieldName));
}

std::vector<uint8_t> OAClient::callMethod(
    zserio::StringView methodName,
    zserio::IServiceData const& requestData,
    void* context)
{
    if (!requestData.getReflectable()) {
        throw std::runtime_error(stx::format("Cannot use OAClient: Make sure that zserio generator call has -withTypeInfoCode flag!"));
    }
    const auto strMethodName = std::string(methodName.begin(), methodName.end());

    auto response = client_.call(strMethodName, [&](const std::string& parameter, const std::string& field, ParameterValueHelper& helper) -> ParameterValue {
        if (field == ZSERIO_REQUEST_PART_WHOLE) {
            zserio::BitBuffer buffer(requestData.getReflectable()->bitSizeOf());
            zserio::BitStreamWriter writer(buffer);
            requestData.getReflectable()->write(writer);
            return helper.binary(std::vector<uint8_t>(buffer.getBuffer(), buffer.getBuffer() + buffer.getByteSize()));
        }
        auto reflectableField = requestData.getReflectable()->find(field);
        if (!reflectableField)
            throw std::runtime_error(stx::format("Could not find field/function for identifier '{}'", field));
        return reflectableToParameterValue(field, reflectableField, reflectableField->getTypeInfo(), helper);
    });

    return {response.begin(), response.end()};
}

}
