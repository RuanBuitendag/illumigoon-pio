#include "animation/Animation.h"
#include <cstring>

AnimationParameter* Animation::findParameter(const std::string& paramName) {
    for (auto& param : parameters) {
        if (paramName == param.name) {
            return &param;
        }
    }
    return nullptr;
}

bool Animation::setParam(const std::string& paramName, int value) {
    AnimationParameter* param = findParameter(paramName);
    if (!param) return false;

    if (param->type == PARAM_INT) {
        *(int*)param->value = value;
        return true;
    } else if (param->type == PARAM_BYTE) {
        if (value >= 0 && value <= 255) {
            *(uint8_t*)param->value = (uint8_t)value;
            return true;
        }
    } else if (param->type == PARAM_FLOAT) {
        *(float*)param->value = (float)value;
        return true;
    }
    return false;
}

bool Animation::setParam(const std::string& paramName, float value) {
    AnimationParameter* param = findParameter(paramName);
    if (!param) return false;

    if (param->type == PARAM_FLOAT) {
        *(float*)param->value = value;
        return true;
    } else if (param->type == PARAM_INT) {
        *(int*)param->value = (int)value;
        return true;
    } else if (param->type == PARAM_BYTE) {
        if (value >= 0 && value <= 255) {
            *(uint8_t*)param->value = (uint8_t)value;
            return true;
        }
    }
    return false;
}

bool Animation::setParam(const std::string& paramName, uint8_t value) {
    AnimationParameter* param = findParameter(paramName);
    if (!param) return false;

    if (param->type == PARAM_BYTE) {
        *(uint8_t*)param->value = value;
        return true;
    } else if (param->type == PARAM_INT) {
        *(int*)param->value = (int)value;
        return true;
    } else if (param->type == PARAM_FLOAT) {
        *(float*)param->value = (float)value;
        return true;
    }
    return false;
}

bool Animation::setParam(const std::string& paramName, bool value) {
    AnimationParameter* param = findParameter(paramName);
    if (param && param->type == PARAM_BOOL) {
        *(bool*)param->value = value;
        return true;
    }
    return false;
}

bool Animation::setParam(const std::string& paramName, CRGB value) {
    AnimationParameter* param = findParameter(paramName);
    if (param && param->type == PARAM_COLOR) {
        *(CRGB*)param->value = value;
        return true;
    }
    return false;
}

bool Animation::setParam(const std::string& paramName, const DynamicPalette& value) {
    AnimationParameter* param = findParameter(paramName);
    if (param && param->type == PARAM_DYNAMIC_PALETTE) {
        *(DynamicPalette*)param->value = value;
        return true;
    }
    return false;
}
