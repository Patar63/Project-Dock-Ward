#include "ColorCorrectionEffect2.h"
#include "Utils/ResourceManager/ResourceManager.h"
#include "Utils/JsonGlmHelpers.h"
#include "Utils/ImGuiHelper.h"
#include "Gameplay/InputEngine.h"

ColorCorrectionEffect2::ColorCorrectionEffect2() :
	ColorCorrectionEffect2(true) { }

ColorCorrectionEffect2::ColorCorrectionEffect2(bool defaultLut2) :
	PostProcessingLayer::Effect(),
	_shader(nullptr),
	_strength3(0.0f),
	Lut3(nullptr)
{
	Name = "Color Correction";
	_format = RenderTargetType::ColorRgb8;

	_shader = ResourceManager::CreateAsset<ShaderProgram>(std::unordered_map<ShaderPartType, std::string>{
		{ ShaderPartType::Vertex, "shaders/vertex_shaders/fullscreen_quad.glsl" },
		{ ShaderPartType::Fragment, "shaders/fragment_shaders/post_effects/color_correction.glsl" }
	});

	if (defaultLut2) {
		Lut3 = ResourceManager::CreateAsset<Texture3D>("luts/warm.cube");

	}

}

ColorCorrectionEffect2::~ColorCorrectionEffect2() = default;

void ColorCorrectionEffect2::Apply(const Framebuffer::Sptr & gBuffer)
{
	_shader->Bind();
	Lut3->Bind(1);
	_shader->SetUniform("u_Strength", _strength3);


}


void ColorCorrectionEffect2::RenderImGui()
{
	LABEL_LEFT(ImGui::LabelText, "LUT 3", Lut3 ? Lut3->GetDebugName().c_str() : "none");
	LABEL_LEFT(ImGui::SliderFloat, "Strength", &_strength3, 0, 1);

}

ColorCorrectionEffect2::Sptr ColorCorrectionEffect2::FromJson(const nlohmann::json & data)
{
	ColorCorrectionEffect2::Sptr result3 = std::make_shared<ColorCorrectionEffect2>(false);
	result3->Enabled = JsonGet(data, "enabled", true);
	result3->_strength3 = JsonGet(data, "strength", result3->_strength3);
	result3->Lut3 = ResourceManager::Get<Texture3D>(Guid(data["lut"].get<std::string>()));
	return result3;
}

nlohmann::json ColorCorrectionEffect2::ToJson() const
{
	return {
		{ "enabled", Enabled },
		{ "lut", Lut3 != nullptr ? Lut3->GetGUID().str() : "null" },
		{ "strength", _strength3 }
	};
}