#pragma once
#include "Application/Layers/PostProcessingLayer.h"
#include "Graphics/ShaderProgram.h"
#include "Graphics/Textures/Texture3D.h"

class ColorCorrectionEffect1 : public PostProcessingLayer::Effect {
public:
	MAKE_PTRS(ColorCorrectionEffect1);
	Texture3D::Sptr Lut2;

	ColorCorrectionEffect1();
	ColorCorrectionEffect1(bool defaultLut1);
	virtual ~ColorCorrectionEffect1();

	virtual void Apply(const Framebuffer::Sptr& gBuffer) override;
	virtual void RenderImGui() override;

	// Inherited from IResource

	ColorCorrectionEffect1::Sptr FromJson(const nlohmann::json& data);
	virtual nlohmann::json ToJson() const override;

protected:
	ShaderProgram::Sptr _shader;
	float _strength2;
};

