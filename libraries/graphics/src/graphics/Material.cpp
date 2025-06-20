//
//  Material.cpp
//  libraries/graphics/src/graphics
//
//  Created by Sam Gateau on 12/10/2014.
//  Copyright 2014 High Fidelity, Inc.
//  Copyright 2024 Overte e.V.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#include "Material.h"

#include "TextureMap.h"

#include <Transform.h>

#include "GraphicsLogging.h"

using namespace graphics;
using namespace gpu;

const float Material::DEFAULT_EMISSIVE { 0.0f };
const float Material::DEFAULT_OPACITY { 1.0f };
const float Material::DEFAULT_ALBEDO { 0.5f };
const float Material::DEFAULT_METALLIC { 0.0f };
const float Material::DEFAULT_ROUGHNESS { 1.0f };
const float Material::DEFAULT_SCATTERING{ 0.0f };
const MaterialKey::OpacityMapMode Material::DEFAULT_OPACITY_MAP_MODE{ MaterialKey::OPACITY_MAP_OPAQUE };
const float Material::DEFAULT_OPACITY_CUTOFF { 0.5f };
const MaterialKey::CullFaceMode Material::DEFAULT_CULL_FACE_MODE { MaterialKey::CULL_BACK };

std::string MaterialKey::getOpacityMapModeName(OpacityMapMode mode) {
    const std::string names[3] = { "OPACITY_MAP_OPAQUE", "OPACITY_MAP_MASK", "OPACITY_MAP_BLEND" };
    return names[mode];
}

bool MaterialKey::getOpacityMapModeFromName(const std::string& modeName, MaterialKey::OpacityMapMode& mode) {
    for (int i = OPACITY_MAP_OPAQUE; i <= OPACITY_MAP_BLEND; i++) {
        mode = (MaterialKey::OpacityMapMode) i;
        if (modeName == getOpacityMapModeName(mode)) {
            return true;
        }
    }
    return false;
}

std::string MaterialKey::getCullFaceModeName(CullFaceMode mode) {
    const std::string names[3] = { "CULL_NONE", "CULL_FRONT", "CULL_BACK" };
    return names[mode];
}

bool MaterialKey::getCullFaceModeFromName(const std::string& modeName, CullFaceMode& mode) {
    for (int i = CULL_NONE; i < NUM_CULL_FACE_MODES; i++) {
        mode = (CullFaceMode)i;
        if (modeName == getCullFaceModeName(mode)) {
            return true;
        }
    }
    return false;
}

const std::string Material::HIFI_PBR { "hifi_pbr" };
const std::string Material::HIFI_SHADER_SIMPLE{ "hifi_shader_simple" };
const std::string Material::VRM_MTOON { "vrm_mtoon" };

Material::Material() {
    for (int i = 0; i < NUM_TOTAL_FLAGS; i++) {
        _propertyFallthroughs[i] = false;
    }
}

Material::Material(const Material& material) :
    _name(material._name),
    _key(material._key),
    _model(material._model),
    _layers(material._layers),
    _emissive(material._emissive),
    _opacity(material._opacity),
    _albedo(material._albedo),
    _roughness(material._roughness),
    _metallic(material._metallic),
    _scattering(material._scattering),
    _opacityCutoff(material._opacityCutoff),
    _texcoordTransforms(material._texcoordTransforms),
    _lightmapParams(material._lightmapParams),
    _materialParams(material._materialParams),
    _cullFaceMode(material._cullFaceMode),
    _textureMaps(material._textureMaps),
    _samplers(material._samplers),
    _texCoordSets(material._texCoordSets),
    _defaultFallthrough(material._defaultFallthrough),
    _propertyFallthroughs(material._propertyFallthroughs)
{
}

Material& Material::operator=(const Material& material) {
    std::lock_guard<std::recursive_mutex> locker(_textureMapsMutex);

    _name = material._name;
    _model = material._model;
    _key = material._key;
    _layers = material._layers;
    _emissive = material._emissive;
    _opacity = material._opacity;
    _albedo = material._albedo;
    _roughness = material._roughness;
    _metallic = material._metallic;
    _scattering = material._scattering;
    _opacityCutoff = material._opacityCutoff;
    _texcoordTransforms = material._texcoordTransforms;
    _lightmapParams = material._lightmapParams;
    _materialParams = material._materialParams;
    _cullFaceMode = material._cullFaceMode;
    _textureMaps = material._textureMaps;
    _samplers = material._samplers;
    _texCoordSets = material._texCoordSets;

    _defaultFallthrough = material._defaultFallthrough;
    _propertyFallthroughs = material._propertyFallthroughs;

    return (*this);
}

void Material::setEmissive(const glm::vec3& emissive, bool isSRGB) {
    _key.setEmissive(glm::any(glm::greaterThan(emissive, glm::vec3(0.0f))));
    _emissive = (isSRGB ? ColorUtils::sRGBToLinearVec3(emissive) : emissive);
}

void Material::setOpacity(float opacity) {
    _key.setTranslucentFactor((opacity < 1.0f));
    _opacity = opacity;
}

void Material::setUnlit(bool value) {
    _key.setUnlit(value);
}

void Material::setAlbedo(const glm::vec3& albedo, bool isSRGB) {
    _key.setAlbedo(true);
    _albedo = (isSRGB ? ColorUtils::sRGBToLinearVec3(albedo) : albedo);
}

void Material::setRoughness(float roughness) {
    roughness = std::min(1.0f, std::max(roughness, 0.0f));
    _key.setGlossy(roughness < 1.0f);
    _roughness = roughness;
}

void Material::setMetallic(float metallic) {
    metallic = glm::clamp(metallic, 0.0f, 1.0f);
    _key.setMetallic(metallic > 0.0f);
    _metallic = metallic;
}

void Material::setScattering(float scattering) {
    scattering = glm::clamp(scattering, 0.0f, 1.0f);
    _key.setScattering(scattering > 0.0f);
    _scattering = scattering;
}

void Material::setOpacityCutoff(float opacityCutoff) {
    opacityCutoff = glm::clamp(opacityCutoff, 0.0f, 1.0f);
    _key.setOpacityCutoff(opacityCutoff != DEFAULT_OPACITY_CUTOFF);
    _opacityCutoff = opacityCutoff;
}

void Material::setOpacityMapMode(MaterialKey::OpacityMapMode opacityMapMode) {
    _key.setOpacityMapMode(opacityMapMode);
}

MaterialKey::OpacityMapMode Material::getOpacityMapMode() const {
    return _key.getOpacityMapMode();
}

void Material::setTextureMap(MapChannel channel, const TextureMapPointer& textureMap) {
    std::lock_guard<std::recursive_mutex>  locker(_textureMapsMutex);

    if (textureMap) {
        _key.setMapChannel(channel, true);
        _textureMaps[channel] = textureMap;
    } else {
        _key.setMapChannel(channel, false);
        _textureMaps.erase(channel);
    }

    if (channel == MaterialKey::ALBEDO_MAP) {
        resetOpacityMap();
        _texcoordTransforms[0] = (textureMap ? textureMap->getTextureTransform().getMatrix() : glm::mat4());
    }

    if (channel == MaterialKey::OCCLUSION_MAP) {
        _texcoordTransforms[1] = (textureMap ? textureMap->getTextureTransform().getMatrix() : glm::mat4());
    }

    if (channel == MaterialKey::LIGHT_MAP) {
        // update the texcoord1 with lightmap
        _texcoordTransforms[1] = (textureMap ? textureMap->getTextureTransform().getMatrix() : glm::mat4());
        _lightmapParams = (textureMap ? glm::vec2(textureMap->getLightmapOffsetScale()) : glm::vec2(0.0, 1.0));
    }

    _materialParams = (textureMap ? glm::vec2(textureMap->getMappingMode(), textureMap->getRepeat()) : glm::vec2(MaterialMappingMode::UV, 1.0));

}

void Material::setSampler(MapChannel channel, const gpu::Sampler& sampler) {
    std::lock_guard<std::recursive_mutex> locker(_textureMapsMutex);
    _samplers[channel] = sampler;
}

void Material::applySampler(MapChannel channel) {
    std::lock_guard<std::recursive_mutex> locker(_textureMapsMutex);

    auto samplerItr = _samplers.find(channel);
    auto textureMapsItr = _textureMaps.find(channel);
    if (samplerItr != _samplers.end() && textureMapsItr != _textureMaps.end() && textureMapsItr->second->getTextureSource()) {
        textureMapsItr->second->getTextureSource()->setSampler(samplerItr->second);
    }
}

void Material::setTexCoordSet(MapChannel channel, int texCoordSet) {
    std::lock_guard<std::recursive_mutex> locker(_textureMapsMutex);
    _texCoordSets[channel] = texCoordSet;
}

int Material::getTexCoordSet(MapChannel channel) {
    std::lock_guard<std::recursive_mutex> locker(_textureMapsMutex);

    auto result = _texCoordSets.find(channel);
    if (result != _texCoordSets.end()) {
        return (result->second);
    } else {
        return 0;
    }
}

bool Material::resetOpacityMap() const {
    // If OpacityMapMode explicit then nothing need to change here.
    if (_key.isOpacityMapMode()) {
        return false;
    }

    // Else, the legacy behavior is to interpret the albedo texture assigned to tune the opacity map mode value
    auto previous = _key.getOpacityMapMode();
    // Clear the previous flags
    _key.setOpacityMaskMap(false);
    _key.setTranslucentMap(false);

    const auto& textureMap = getTextureMap(MaterialKey::ALBEDO_MAP);
    if (textureMap &&
        textureMap->useAlphaChannel() &&
        textureMap->isDefined() &&
        textureMap->getTextureView().isValid()) {

        auto usage = textureMap->getTextureView()._texture->getUsage();
        if (usage.isAlpha()) {
            if (usage.isAlphaMask()) {
                // Texture has alpha, but it is just a mask
                _key.setOpacityMaskMap(true);
                _key.setTranslucentMap(false);
            } else {
                // Texture has alpha, it is a true translucency channel
                _key.setOpacityMaskMap(false);
                _key.setTranslucentMap(true);
            }
        }
    }
    if (previous != _key.getOpacityMapMode()) {
        //opacity change detected for this material
        return true;
    }
    return false;
}

const TextureMapPointer Material::getTextureMap(MapChannel channel) const {
    std::lock_guard<std::recursive_mutex> locker(_textureMapsMutex);

    auto result = _textureMaps.find(channel);
    if (result != _textureMaps.end()) {
        return (result->second);
    } else {
        return TextureMapPointer();
    }
}

void Material::setTextureTransforms(const Transform& transform, MaterialMappingMode mode, bool repeat) {
    for (auto &textureMapItem : _textureMaps) {
        if (textureMapItem.second) {
            textureMapItem.second->setTextureTransform(transform);
            textureMapItem.second->setMappingMode(mode);
            textureMapItem.second->setRepeat(repeat);
        }
    }
    for (int i = 0; i < NUM_TEXCOORD_TRANSFORMS; i++) {
        _texcoordTransforms[i] = transform.getMatrix();
    }
    _materialParams = glm::vec2(mode, repeat);
}

const glm::vec3 Material::DEFAULT_SHADE = glm::vec3(0.0f);
const float Material::DEFAULT_SHADING_SHIFT = 0.0f;
const float Material::DEFAULT_SHADING_TOONY = 0.9f;
const glm::vec3 Material::DEFAULT_MATCAP = glm::vec3(1.0f);
const glm::vec3 Material::DEFAULT_PARAMETRIC_RIM = glm::vec3(0.0f);
const float Material::DEFAULT_PARAMETRIC_RIM_FRESNEL_POWER = 5.0f;
const float Material::DEFAULT_PARAMETRIC_RIM_LIFT = 0.0f;
const float Material::DEFAULT_RIM_LIGHTING_MIX = 1.0f;
const float Material::DEFAULT_UV_ANIMATION_SCROLL_SPEED = 0.0f;
const glm::vec3 Material::DEFAULT_OUTLINE = glm::vec3(0.0f);

MultiMaterial::MultiMaterial() {
    Schema schema;
    _schemaBuffer = gpu::BufferView(std::make_shared<gpu::Buffer>(sizeof(Schema), (const gpu::Byte*) &schema, sizeof(Schema)));
    for (int i = 0; i < _textureTables.size(); i++) {
        _textureTables[i] = std::make_shared<gpu::TextureTable>();
    }
}

void MultiMaterial::calculateMaterialInfo() const {
    if (!_hasCalculatedTextureInfo) {
        bool allTextures = true; // assume we got this...
        _textureSize = 0;
        _textureCount = 0;

        for (uint8_t i = 0; i < _layers; i++) {
            auto textures = _textureTables[i]->getTextures();
            for (auto const& texture : textures) {
                if (texture && texture->isDefined()) {
                    auto size = texture->getSize();
                    _textureSize += size;
                    _textureCount++;
                } else {
                    allTextures = false;
                }
            }
        }
        _hasCalculatedTextureInfo = allTextures;
    }
}

void MultiMaterial::resetReferenceTexturesAndMaterials() {
    _referenceTextures.clear();
    _referenceMaterials.clear();
}

void MultiMaterial::addReferenceTexture(const std::function<gpu::TexturePointer()>& textureOperator) {
    _referenceTextures.emplace_back(textureOperator, textureOperator());
}

void MultiMaterial::addReferenceMaterial(const std::function<graphics::MaterialPointer()>& materialOperator) {
    _referenceMaterials.emplace_back(materialOperator, materialOperator());
}

bool MultiMaterial::anyReferenceMaterialsOrTexturesChanged() const {
    for (auto textureOperatorPair : _referenceTextures) {
        if (textureOperatorPair.first() != textureOperatorPair.second) {
            return true;
        }
    }

    for (auto materialOperatorPair : _referenceMaterials) {
        if (materialOperatorPair.first() != materialOperatorPair.second) {
            return true;
        }
    }

    return false;
}

void MultiMaterial::setisMToonAndLayers(bool isMToon, uint8_t layers) {
    if (isMToon != _isMToon || layers != _layers) {
        _isMToon = isMToon;
        _layers = layers;

        if (isMToon) {
            std::array<MToonSchema, 3> toonSchemas;
            size_t size = _layers * sizeof(MToonSchema);
            _schemaBuffer = gpu::BufferView(std::make_shared<gpu::Buffer>(size, (const gpu::Byte*)toonSchemas.data(), size));
        } else {
            std::array<Schema, 3> schemas;
            size_t size = _layers * sizeof(Schema);
            _schemaBuffer = gpu::BufferView(std::make_shared<gpu::Buffer>(size, (const gpu::Byte*)schemas.data(), size));
        }
    }
}

void MultiMaterial::setMToonTime() {
    assert(_isMToon);

    // Some objects, like material entities, don't have persistent MultiMaterials to store this in, so we just store it once statically
    static uint64_t mtoonStartTime;
    static std::once_flag once;
    std::call_once(once, [] {
        mtoonStartTime = usecTimestampNow();
    });

    // Minimize floating point error by doing an integer division to milliseconds, before the floating point division to seconds
    float mtoonTime = (float)((usecTimestampNow() - mtoonStartTime) / USECS_PER_MSEC) / MSECS_PER_SECOND;
    // MToon time is only stored in the first material
    _schemaBuffer.edit<graphics::MultiMaterial::MToonSchema>()._time = mtoonTime;
}

void MultiMaterial::applySamplers() const {
    for (auto& func : _samplerFuncs) {
        func();
    }
}
