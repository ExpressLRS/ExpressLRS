#include <algorithm>

#include <ArduinoJson.h>
#include <AsyncJson.h>
#include <ESPAsyncWebServer.h>

#include "CRSFEndpoint.h"
#include "CRSFParameters.h"
#include "TXModuleEndpoint.h"
#include "config.h"
#include "device.h"

static void appendSelectionOptions(const char *options, JsonArray outArray, const bool keepReservedSlots = true)
{
  static constexpr const char *RESERVED_OPTION_LABEL = "(reserved)";

  if (options == nullptr)
  {
    return;
  }

  const char *cursor = options;
  while (*cursor != '\0')
  {
    const char *end = cursor;
    while (*end != '\0' && *end != ';')
    {
      ++end;
    }

    if (end > cursor)
    {
      const size_t len = end - cursor;
      char entry[64] = {};
      strlcpy(entry, cursor, std::min(len + 1, sizeof(entry)));

      char displayEntry[96] = {};
      size_t out = 0;
      for (size_t in = 0; entry[in] != '\0' && out + 1 < sizeof(displayEntry); ++in)
      {
        const uint8_t ch = (uint8_t)entry[in];
        if (ch == 0xC0)
        {
          displayEntry[out++] = ' ';
          displayEntry[out++] = 'U';
          displayEntry[out++] = 'p';
        }
        else if (ch == 0xC1)
        {
          displayEntry[out++] = ' ';
          displayEntry[out++] = 'D';
          displayEntry[out++] = 'o';
          displayEntry[out++] = 'w';
          displayEntry[out++] = 'n';
        }
        else
        {
          displayEntry[out++] = ch;
        }
      }
      displayEntry[out] = '\0';

      if (displayEntry[0] == '\0')
      {
        if (keepReservedSlots)
        {
          outArray.add(RESERVED_OPTION_LABEL);
        }
      }
      else
      {
        outArray.add(displayEntry);
      }
    }
    else if (keepReservedSlots)
    {
      outArray.add(RESERVED_OPTION_LABEL);
    }

    cursor = end;
    if (*cursor == ';')
    {
      cursor++;
    }
  }
}

static void appendFolderChildren(const CRSFEndpoint *endpoint, const uint8_t parentId, JsonArray children)
{
  endpoint->forEachParameter([&](const uint8_t id, const propertiesCommon *child) {
    if (child->parent == parentId)
    {
      children.add(id);
    }
  });
}

static const char *crsfTypeName(const uint8_t dataType)
{
  switch (dataType)
  {
    case CRSF_UINT8: return "uint8";
    case CRSF_INT8: return "int8";
    case CRSF_UINT16: return "uint16";
    case CRSF_INT16: return "int16";
    case CRSF_UINT32: return "uint32";
    case CRSF_INT32: return "int32";
    case CRSF_UINT64: return "uint64";
    case CRSF_INT64: return "int64";
    case CRSF_FLOAT: return "float";
    case CRSF_TEXT_SELECTION: return "selection";
    case CRSF_STRING: return "string";
    case CRSF_FOLDER: return "folder";
    case CRSF_INFO: return "info";
    case CRSF_COMMAND: return "command";
    case CRSF_VTX: return "vtx";
    default: return "unknown";
  }
}

static float decodeWithPrecision(const int32_t rawValue, const uint8_t precision)
{
  float scale = 1.0f;
  for (uint8_t i = 0; i < precision; i++)
  {
    scale *= 10.0f;
  }
  return rawValue / scale;
}

static void HandleGetLuaParameters(AsyncWebServerRequest *request)
{
  CRSFEndpoint *endpoint = &crsfTransmitter;
  const char *endpointName = "tx";

  if (request->hasArg("modelId"))
  {
    long requestedModelId = request->arg("modelId").toInt();
    if (requestedModelId < 0)
      requestedModelId = 0;
    if (requestedModelId > 63)
      requestedModelId = 63;

    const uint8_t modelId = (uint8_t)requestedModelId;
    const bool modelChanged = (crsfTransmitter.modelId != modelId);
    crsfTransmitter.modelId = modelId;
    config.SetModelId(modelId);
    if (modelChanged)
    {
      ModelUpdateReq();
    }
  }

  endpoint->updateParameters();

  auto *response = new AsyncJsonResponse();
  JsonObject root = response->getRoot().to<JsonObject>();
  root["endpoint"] = endpointName;
  root["source"] = "tx-lua";
  root["format"] = "crsf-parameter-parsed";
  root["modelId"] = crsfTransmitter.modelId;

  JsonArray params = root["parameters"].to<JsonArray>();

  endpoint->forEachParameter([&](const uint8_t id, const propertiesCommon *p) {
    const uint8_t dataType = p->type & CRSF_FIELD_TYPE_MASK;
    JsonObject param = params.add<JsonObject>();
    param["id"] = id;
    param["parent"] = p->parent;
    param["name"] = p->name ? p->name : "";
    param["type-id"] = dataType;
    param["type"] = crsfTypeName(dataType);
    param["hidden"] = (p->type & CRSF_FIELD_HIDDEN) != 0;
    param["elrs-hidden"] = (p->type & CRSF_FIELD_ELRS_HIDDEN) != 0;

    switch (dataType)
    {
    case CRSF_TEXT_SELECTION:
    {
      const selectionParameter *sel = reinterpret_cast<const selectionParameter *>(p);
      int32_t webValue = sel->value;
      const bool mappedForWeb = crsfTransmitter.getSelectionValueForWeb(id, webValue);
      param["value"] = webValue;
      param["web-compact-index"] = mappedForWeb;
      param["units"] = sel->units ? sel->units : "";
      JsonArray optionsArray = param["options"].to<JsonArray>();
      appendSelectionOptions(sel->options, optionsArray, !mappedForWeb);

      char selected[64] = {};
      findSelectionLabel(sel, selected, sel->value);
      param["selected"] = selected;
      break;
    }
    case CRSF_COMMAND:
    {
      const auto *cmd = reinterpret_cast<const commandParameter *>(p);
      param["step"] = cmd->step;
      param["info"] = cmd->info ? cmd->info : "";
      break;
    }
    case CRSF_UINT8:
    case CRSF_INT8:
    {
      const int8Parameter *i8 = reinterpret_cast<const int8Parameter *>(p);
      if (dataType == CRSF_UINT8)
      {
        param["value"] = i8->properties.u.value;
        param["min"] = i8->properties.u.min;
        param["max"] = i8->properties.u.max;
      }
      else
      {
        param["value"] = i8->properties.s.value;
        param["min"] = i8->properties.s.min;
        param["max"] = i8->properties.s.max;
      }
      param["units"] = i8->units ? i8->units : "";
      break;
    }
    case CRSF_UINT16:
    case CRSF_INT16:
    {
      const int16Parameter *i16 = reinterpret_cast<const int16Parameter *>(p);
      if (dataType == CRSF_UINT16)
      {
        param["value"] = be16toh(i16->properties.u.value);
        param["min"] = i16->properties.u.min;
        param["max"] = i16->properties.u.max;
      }
      else
      {
        param["value"] = (int16_t)be16toh(i16->properties.u.value);
        param["min"] = i16->properties.s.min;
        param["max"] = i16->properties.s.max;
      }
      param["units"] = i16->units ? i16->units : "";
      break;
    }
    case CRSF_FLOAT:
    {
      const floatParameter *flt = reinterpret_cast<const floatParameter *>(p);
      const int32_t valueRaw = (int32_t)be32toh(flt->properties.value);
      const int32_t minRaw = (int32_t)be32toh(flt->properties.min);
      const int32_t maxRaw = (int32_t)be32toh(flt->properties.max);
      const int32_t defRaw = (int32_t)be32toh(flt->properties.def);
      const int32_t stepRaw = (int32_t)be32toh(flt->properties.step);

      param["precision"] = flt->properties.precision;
      param["value-raw"] = valueRaw;
      param["min-raw"] = minRaw;
      param["max-raw"] = maxRaw;
      param["default-raw"] = defRaw;
      param["step-raw"] = stepRaw;
      param["value"] = decodeWithPrecision(valueRaw, flt->properties.precision);
      param["min"] = decodeWithPrecision(minRaw, flt->properties.precision);
      param["max"] = decodeWithPrecision(maxRaw, flt->properties.precision);
      param["default"] = decodeWithPrecision(defRaw, flt->properties.precision);
      param["step"] = decodeWithPrecision(stepRaw, flt->properties.precision);
      param["units"] = flt->units ? flt->units : "";
      break;
    }
    case CRSF_STRING:
    case CRSF_INFO:
    {
      const auto *str = reinterpret_cast<const stringParameter *>(p);
      param["value"] = str->value ? str->value : "";
      break;
    }
    case CRSF_FOLDER:
    {
      const auto *folder = reinterpret_cast<const folderParameter *>(p);
      if (folder->dyn_name != nullptr)
      {
        param["name"] = folder->dyn_name;
      }
      JsonArray children = param["children"].to<JsonArray>();
      appendFolderChildren(endpoint, id, children);
      break;
    }
    default:
      break;
    }
  });

  response->setLength();
  request->send(response);
}

static void HandleSaveLuaParameters(AsyncWebServerRequest *request, JsonVariant &json)
{
  if (!json.is<JsonObject>())
  {
    request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }

  JsonObject obj = json.as<JsonObject>();

  JsonVariant modelIdVariant = obj["modelId"];
  if (!modelIdVariant.isNull())
  {
    int modelId = modelIdVariant | -1;
    if (modelId >= 0 && modelId < 64)
    {
      config.SetModelId(modelId);
      ModelUpdateReq();
    }
  }

  JsonVariant changesVariant = obj["changes"];
  if (!changesVariant.is<JsonArray>())
  {
    request->send(400, "application/json", "{\"error\":\"Missing or invalid 'changes' array\"}");
    return;
  }

  JsonArray changes = changesVariant.as<JsonArray>();
  uint32_t applied = 0;
  uint32_t skipped = 0;

  for (JsonVariant change : changes)
  {
    if (!change.is<JsonObject>())
    {
      skipped++;
      continue;
    }

    JsonObject ch = change.as<JsonObject>();
    JsonVariant idVariant = ch["id"];
    JsonVariant valueVariant = ch["value"];
    if (idVariant.isNull() || valueVariant.isNull())
    {
      skipped++;
      continue;
    }

    uint8_t id = idVariant;
    int32_t value = valueVariant;

    const propertiesCommon *parameter = crsfTransmitter.getParameterById(id);
    if (!parameter)
    {
      skipped++;
      continue;
    }

    const uint8_t dataType = parameter->type & CRSF_FIELD_TYPE_MASK;
    if (dataType == CRSF_STRING || dataType == CRSF_INFO || dataType == CRSF_COMMAND || dataType == CRSF_FOLDER)
    {
      skipped++;
      continue;
    }

    if (dataType == CRSF_FLOAT)
    {
      const auto *flt = reinterpret_cast<const floatParameter *>(parameter);
      float in = valueVariant.as<float>();
      float scale = 1.0f;
      for (uint8_t i = 0; i < flt->properties.precision; i++)
      {
        scale *= 10.0f;
      }
      value = (int32_t)(in * scale);
    }

    if (crsfTransmitter.writeParameterValueForWeb(id, value))
    {
      applied++;
    }
    else
    {
      skipped++;
    }
  }

  const uint32_t changeFlags = config.Commit();
  if (changeFlags != 0)
  {
    devicesTriggerEvent(changeFlags);
  }

  auto *response = new AsyncJsonResponse();
  JsonObject root = response->getRoot().to<JsonObject>();
  root["status"] = "ok";
  root["applied"] = applied;
  root["skipped"] = skipped;
  root["changeFlags"] = changeFlags;
  response->setLength();
  request->send(response);
}

void registerTxLuaParameterHandlers(AsyncWebServer &server)
{
  server.on("/lua-parameters", HTTP_GET, HandleGetLuaParameters);
  server.addHandler(new AsyncCallbackJsonWebHandler("/lua-parameters/save", HandleSaveLuaParameters));
}
