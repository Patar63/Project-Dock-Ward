#pragma once
#include "Application/Layers/PostProcessingLayer.h"
#include "Graphics/ShaderProgram.h"
#include "Graphics/Textures/Texture3D.h"

class ColorCorrectionEffect2 : public PostProcessingLayer::Effect {
public:
	MAKE_PTRS(ColorCorrectionEffect2);
	Texture3D::Sptr Lut3;

	ColorCorrectionEffect2();
	ColorCorrectionEffect2(bool defaultLut2);
	virtual ~ColorCorrectionEffect2();

	virtual void Apply(const Framebuffer::Sptr& gBuffer) override;
	virtual void RenderImGui() override;

	// Inherited from IResource

	ColorCorrectionEffect2::Sptr FromJson(const nlohmann::json& data);
	virtual nlohmann::json ToJson() const override;

protected:
	ShaderProgram::Sptr _shader;
	float _strength3;
};

