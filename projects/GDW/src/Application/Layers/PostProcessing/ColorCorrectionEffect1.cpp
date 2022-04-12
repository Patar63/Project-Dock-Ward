#include "ColorCorrectionEffect1.h"
#include "Utils/ResourceManager/ResourceManager.h"
#include "Utils/JsonGlmHelpers.h"
#include "Utils/ImGuiHelper.h"
#include "Gameplay/InputEngine.h"

ColorCorrectionEffect1::ColorCorrectionEffect1() :
	ColorCorrectionEffect1(true) { }

ColorCorrectionEffect1::ColorCorrectionEffect1(bool defaultLut1) :
	PostProcessingLayer::Effect(),
	_shader(nullptr),
	_strength2(0.0f),
	Lut2(nullptr)
{
	Name = "Color Correction";
	_format = RenderTargetType::ColorRgb8;

	_shader = ResourceManager::CreateAsset<ShaderProgram>(std::unordered_map<ShaderPartType, std::string>{
		{ ShaderPartType::Vertex, "shaders/vertex_shaders/fullscreen_quad.glsl" },
		{ ShaderPartType::Fragment, "shaders/fragment_shaders/post_effects/color_correction.glsl" }
	});

	if (defaultLut1) {
		Lut2 = ResourceManager::CreateAsset<Texture3D>("luts/cool.cube");

	}

}

ColorCorrectionEffect1::~ColorCorrectionEffect1() = default;

void ColorCorrectionEffect1::Apply(const Framebuffer::Sptr & gBuffer)
{
	_shader->Bind();
	Lut2->Bind(1);
	_shader->SetUniform("u_Strength", _strength2);


}


void ColorCorrectionEffect1::RenderImGui()
{
	LABEL_LEFT(ImGui::LabelText, "LUT 2", Lut2 ? Lut2->GetDebugName().c_str() : "none");
	LABEL_LEFT(ImGui::SliderFloat, "Strength", &_strength2, 0, 1);

}

ColorCorrectionEffect1::Sptr ColorCorrectionEffect1::FromJson(const nlohmann::json & data)
{
	ColorCorrectionEffect1::Sptr result2 = std::make_shared<ColorCorrectionEffect1>(false);
	result2->Enabled = JsonGet(data, "enabled", true);
	result2->_strength2 = JsonGet(data, "strength", result2->_strength2);
	result2->Lut2 = ResourceManager::Get<Texture3D>(Guid(data["lut"].get<std::string>()));
	return result2;
}

nlohmann::json ColorCorrectionEffect1::ToJson() const
{
	return {
		{ "enabled", Enabled },
		{ "lut", Lut2 != nullptr ? Lut2->GetGUID().str() : "null" },
		{ "strength", _strength2 }
	};
}