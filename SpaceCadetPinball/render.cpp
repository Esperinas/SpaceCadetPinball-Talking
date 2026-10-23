#include "pch.h"
#include "render.h"

#include "fullscrn.h"
#include "GroupData.h"
#include "options.h"
#include "pb.h"
#include "score.h"
#include "TTextBox.h"
#include "TPinballTable.h"
#include "winmain.h"
#include "DebugOverlay.h"
#include "proj.h"

std::vector<render_sprite*> render::sprite_list, render::ball_list;
zmap_header_type* render::background_zmap;
int render::zmap_offsetX, render::zmap_offsetY, render::offset_x, render::offset_y;
rectangle_type render::vscreen_rect;
gdrv_bitmap8 *render::vscreen, *render::background_bitmap, *render::ball_bitmap[20];
zmap_header_type* render::zscreen;
SDL_Rect render::DestinationRect{};
SDL_Texture* render::spaceCadetGlowTexture = nullptr;
SDL_Texture* render::customRightPanelTexture = nullptr;
SDL_Texture* render::customRightPanelHdTexture = nullptr;
SDL_Rect render::spaceCadetGlowRect{};

render_sprite::render_sprite(VisualTypes visualType, gdrv_bitmap8* bmp, zmap_header_type* zMap,
	int xPosition, int yPosition, rectangle_type* boundingRect)
{
	Bmp = bmp;
	ZMap = zMap;
	VisualType = visualType;
	DeleteFlag = false;
	OccludedSprites = nullptr;
	DirtyRect = rectangle_type{};
	DirtyFlag = visualType != VisualTypes::Ball;
	ZMapOffestX = 0;
	ZMapOffestY = 0;
	Depth = 0xffFF;

	if (boundingRect)
	{
		BoundingRect = *boundingRect;
	}
	else
	{
		BoundingRect.Width = -1;
		BoundingRect.Height = -1;
		BoundingRect.XPosition = 0;
		BoundingRect.YPosition = 0;
	}

	BmpRect.XPosition = xPosition;
	BmpRect.YPosition = yPosition;
	if (bmp)
	{
		BmpRect.Width = bmp->Width;
		BmpRect.Height = bmp->Height;
	}
	else
	{
		BmpRect.Width = 0;
		BmpRect.Height = 0;
	}
	DirtyRectPrev = BmpRect;

	if (!ZMap && VisualType != VisualTypes::Ball)
	{
		assertm(false, "Background zMap should not be used");
		ZMap = render::background_zmap;
		ZMapOffestY = xPosition - render::zmap_offsetX;
		ZMapOffestX = yPosition - render::zmap_offsetY;
	}

	render::AddSprite(*this);
}

render_sprite::~render_sprite()
{
	render::RemoveSprite(*this);
	delete OccludedSprites;
}

void render_sprite::set(gdrv_bitmap8* bmp, zmap_header_type* zMap, int xPos, int yPos)
{
	if (Bmp == bmp && ZMap == zMap && BmpRect.XPosition == xPos && BmpRect.YPosition == yPos)
		return;

	Bmp = bmp;
	ZMap = zMap;
	DirtyFlag = VisualType != VisualTypes::Ball;
	BmpRect.XPosition = xPos;
	BmpRect.YPosition = yPos;
	if (bmp) 
	{
		BmpRect.Width = bmp->Width;
		BmpRect.Height = bmp->Height;
	}
}

void render_sprite::ball_set(gdrv_bitmap8* bmp, float depth, int xPos, int yPos)
{
	set(bmp, ZMap,xPos, yPos);
	Depth = proj::NormalizeDepth(depth);
}

void render::init(gdrv_bitmap8* bmp, int width, int height)
{
	vscreen = new gdrv_bitmap8(width, height, false);
	zscreen = new zmap_header_type(width, height, width);
	zdrv::fill(zscreen, zscreen->Width, zscreen->Height, 0, 0, 0xFFFF);
	vscreen_rect.YPosition = 0;
	vscreen_rect.XPosition = 0;
	vscreen_rect.Width = width;
	vscreen_rect.Height = height;
	vscreen->YPosition = 0;
	vscreen->XPosition = 0;
	for (auto& ballBmp : ball_bitmap)
		ballBmp = new gdrv_bitmap8(64, 64, false);

	background_bitmap = bmp;
	if (bmp)
		gdrv::copy_bitmap(vscreen, width, height, 0, 0, bmp, 0, 0);
	else
		gdrv::fill_bitmap(vscreen, vscreen->Width, vscreen->Height, 0, 0, 0);

	recreate_screen_texture();
}

void render::uninit()
{
	if (customRightPanelTexture)
	{
		SDL_DestroyTexture(customRightPanelTexture);
		customRightPanelTexture = nullptr;
	}
	if (customRightPanelHdTexture)
	{
		SDL_DestroyTexture(customRightPanelHdTexture);
		customRightPanelHdTexture = nullptr;
	}
	if (spaceCadetGlowTexture)
	{
		SDL_DestroyTexture(spaceCadetGlowTexture);
		spaceCadetGlowTexture = nullptr;
	}
	delete vscreen;
	delete zscreen;

	// Sprite destructor removes it from the list.
	while (!sprite_list.empty())
		delete sprite_list[0];
	while (!ball_list.empty())
		delete ball_list[0];
	for (auto& ballBmp : ball_bitmap)
		delete ballBmp;
	DebugOverlay::UnInit();
}

void render::recreate_screen_texture()
{
	// Enhanced 3.0 deliberately uses a single direct nearest-neighbour pass.
	// This keeps the original bitmap pixels crisp at high display resolutions and,
	// unlike the former CPU filter and 2x render target, adds no per-frame pass.
	vscreen->CreateTexture(options::Options.EnhancedScaling ? "nearest" :
	                      (options::Options.LinearFiltering ? "linear" : "nearest"),
	                      SDL_TEXTUREACCESS_STREAMING);
}

void render::update()
{
	unpaint_balls();

	// Clip dirty sprites with vScreen, clear clipping (dirty) rectangles
	for (const auto sprite : sprite_list)
	{
		if (!sprite->DirtyFlag)
			continue;

		bool clearSprite = false;
		switch (sprite->VisualType)
		{
		case VisualTypes::Sprite:
			if (sprite->DirtyRectPrev.Width > 0)
				maths::enclosing_box(sprite->DirtyRectPrev, sprite->BmpRect, sprite->DirtyRect);

			if (maths::rectangle_clip(sprite->DirtyRect, vscreen_rect, &sprite->DirtyRect))
				clearSprite = true;
			else
				sprite->DirtyRect.Width = -1;
			break;
		case VisualTypes::Background:
			if (maths::rectangle_clip(sprite->BmpRect, vscreen_rect, &sprite->DirtyRect))
				clearSprite = !sprite->Bmp;
			else
				sprite->DirtyRect.Width = -1;
			break;
		default: break;
		}

		if (clearSprite)
		{
			auto yPos = sprite->DirtyRect.YPosition;
			auto width = sprite->DirtyRect.Width;
			auto xPos = sprite->DirtyRect.XPosition;
			auto height = sprite->DirtyRect.Height;
			zdrv::fill(zscreen, width, height, xPos, yPos, 0xFFFF);
			if (background_bitmap)
				gdrv::copy_bitmap(vscreen, width, height, xPos, yPos, background_bitmap, xPos, yPos);
			else
				gdrv::fill_bitmap(vscreen, width, height, xPos, yPos, 0);
		}
	}

	// Paint dirty rectangles of dirty sprites
	for (auto sprite : sprite_list)
	{
		if (!sprite->DirtyFlag)
			continue;

		repaint(*sprite);
		sprite->DirtyFlag = false;
		sprite->DirtyRectPrev = sprite->DirtyRect;
		if (sprite->DeleteFlag)
			delete sprite;
	}

	paint_balls();
}

void render::AddSprite(render_sprite& sprite)
{
	auto& list = sprite.VisualType == VisualTypes::Ball ? ball_list : sprite_list;
	list.push_back(&sprite);
}

void render::RemoveSprite(render_sprite& sprite)
{
	auto& list = sprite.VisualType == VisualTypes::Ball ? ball_list : sprite_list;
	auto it = std::find(list.begin(), list.end(), &sprite);
	if (it != list.end())
		list.erase(it);
}

void render::set_background_zmap(zmap_header_type* zMap, int offsetX, int offsetY)
{
	background_zmap = zMap;
	zmap_offsetX = offsetX;
	zmap_offsetY = offsetY;
}

void render::repaint(const render_sprite& sprite)
{
	rectangle_type clipRect{};
	if (!sprite.OccludedSprites || sprite.VisualType == VisualTypes::Ball || sprite.DirtyRect.Width <= 0)
		return;

	for (auto refSprite : *sprite.OccludedSprites)
	{
		if (!refSprite->DeleteFlag && refSprite->Bmp)
		{
			if (maths::rectangle_clip(refSprite->BmpRect, sprite.DirtyRect, &clipRect))
				zdrv::paint(
					clipRect.Width,
					clipRect.Height,
					vscreen,
					clipRect.XPosition,
					clipRect.YPosition,
					zscreen,
					clipRect.XPosition,
					clipRect.YPosition,
					refSprite->Bmp,
					clipRect.XPosition - refSprite->BmpRect.XPosition,
					clipRect.YPosition - refSprite->BmpRect.YPosition,
					refSprite->ZMap,
					clipRect.XPosition + refSprite->ZMapOffestY - refSprite->BmpRect.XPosition,
					clipRect.YPosition + refSprite->ZMapOffestX - refSprite->BmpRect.YPosition);
		}
	}
}


void render::paint_balls()
{
	// Sort ball sprites by ascending depth
	std::sort(ball_list.begin(), ball_list.end(), [](const render_sprite* lhs, const render_sprite* rhs)
	{
		return lhs->Depth < rhs->Depth;
	});

	// For balls that clip vScreen: save original vScreen contents and paint ball bitmap.
	for (auto index = 0u; index < ball_list.size(); ++index)
	{
		auto ball = ball_list[index];
		auto dirty = &ball->DirtyRect;
		if (ball->Bmp && maths::rectangle_clip(ball->BmpRect, vscreen_rect, &ball->DirtyRect))
		{
			int xPos = dirty->XPosition;
			int yPos = dirty->YPosition;
			gdrv::copy_bitmap(ball_bitmap[index], dirty->Width, dirty->Height, 0, 0, vscreen, xPos, yPos);
			zdrv::paint_flat(
				dirty->Width,
				dirty->Height,
				vscreen,
				xPos,
				yPos,
				zscreen,
				xPos,
				yPos,
				ball->Bmp,
				xPos - ball->BmpRect.XPosition,
				yPos - ball->BmpRect.YPosition,
				ball->Depth);
		}
		else
		{
			dirty->Width = -1;
		}
	}
}

void render::unpaint_balls()
{
	// Restore portions of vScreen saved during previous paint_balls call.
	for (int index = static_cast<int>(ball_list.size()) - 1; index >= 0; index--)
	{
		auto curBall = ball_list[index];
		if (curBall->DirtyRect.Width > 0)
			gdrv::copy_bitmap(
				vscreen,
				curBall->DirtyRect.Width,
				curBall->DirtyRect.Height,
				curBall->DirtyRect.XPosition,
				curBall->DirtyRect.YPosition,
				ball_bitmap[index],
				0,
				0);

		curBall->DirtyRectPrev = curBall->DirtyRect;
	}
}

void render::shift(int offsetX, int offsetY)
{
	offset_x += offsetX;
	offset_y += offsetY;
}

void render::build_occlude_list()
{
	std::vector<render_sprite*>* spriteArr = nullptr;
	for (auto mainSprite : sprite_list)
	{
		if (mainSprite->OccludedSprites)
		{
			delete mainSprite->OccludedSprites;
			mainSprite->OccludedSprites = nullptr;
		}

		if (!mainSprite->DeleteFlag && mainSprite->BoundingRect.Width != -1)
		{
			if (!spriteArr)
				spriteArr = new std::vector<render_sprite*>();

			for (auto refSprite : sprite_list)
			{
				if (!refSprite->DeleteFlag
					&& refSprite->BoundingRect.Width != -1
					&& maths::rectangle_clip(mainSprite->BoundingRect, refSprite->BoundingRect, nullptr))
				{
					spriteArr->push_back(refSprite);
				}
			}

			if (mainSprite->Bmp && spriteArr->size() < 2)
				spriteArr->clear();
			if (!spriteArr->empty())
			{
				mainSprite->OccludedSprites = spriteArr;
				spriteArr = nullptr;
			}
		}
	}

	delete spriteArr;
}

void render::SpriteViewer(bool* show)
{
	static const char* BitmapTypes[] =
	{
		"None",
		"RawBitmap",
		"DibBitmap",
		"Spliced",
	};
	static float scale = 1.0f;
	auto uv_min = ImVec2(0.0f, 0.0f); // Top-left
	auto uv_max = ImVec2(1.0f, 1.0f); // Lower-right
	auto tint_col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // No tint
	auto border_col = ImVec4(1.0f, 1.0f, 1.0f, 0.5f); // 50% opaque white

	if (ImGui::Begin("Sprite viewer", show, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_MenuBar))
	{
		if (ImGui::BeginMenuBar())
		{
			ImGui::SliderFloat("Sprite scale", &scale, 0.1f, 10.0f, "scale = %.3f");
			ImGui::EndMenuBar();
		}

		for (const auto group : pb::record_table->Groups)
		{
			bool emptyGroup = true;
			for (int i = 0; i <= 2; i++)
			{
				auto bmp = group->GetBitmap(i);
				if (bmp)
				{
					emptyGroup = false;
					break;
				}
			}
			if (emptyGroup)
				continue;

			ImGui::Text("Group: %d, name:%s", group->GroupId, group->GroupName.c_str());
			for (int i = 0; i <= 2; i++)
			{
				auto bmp = group->GetBitmap(i);
				if (!bmp)
					continue;

				auto type = BitmapTypes[static_cast<uint8_t>(bmp->BitmapType)];
				ImGui::Text("type:%s, size:%d, resolution: %dx%d, offset:%dx%d", type,
				            bmp->Resolution,
				            bmp->Width, bmp->Height, bmp->XPosition, bmp->YPosition);
			}

			for (int same = 0, i = 0; i <= 2; i++)
			{
				auto bmp = group->GetBitmap(i);
				if (!bmp)
					continue;

				gdrv::CreatePreview(*bmp);
				if (bmp->Texture)
				{
					if (!same)
						same = true;
					else
						ImGui::SameLine();

					ImGui::Image(bmp->Texture, ImVec2(bmp->Width * scale, bmp->Height * scale),
					             uv_min, uv_max, tint_col, border_col);
				}
			}

			for (int same = 0, i = 0; i <= 2; i++)
			{
				auto zMap = group->GetZMap(i);
				if (!zMap)
					continue;

				zdrv::CreatePreview(*zMap);
				if (zMap->Texture)
				{
					if (!same)
						same = true;
					else
						ImGui::SameLine();
					ImGui::Image(zMap->Texture, ImVec2(zMap->Width * scale, zMap->Height * scale),
					             uv_min, uv_max, tint_col, border_col);
				}
			}
		}
	}
	ImGui::End();
}

void render::PresentTexture(SDL_Texture* texture)
{
	if (offset_x == 0 && offset_y == 0)
	{
		SDL_RenderCopy(winmain::Renderer, texture, nullptr, &DestinationRect);
	}
	else
	{
		auto tableWidthCoef = static_cast<float>(pb::MainTable->Width) / vscreen->Width;
		auto srcSeparationX = static_cast<int>(round(vscreen->Width * tableWidthCoef));
		auto srcBoardRect = SDL_Rect
		{
			0, 0,
			srcSeparationX, vscreen->Height
		};
		auto srcSidebarRect = SDL_Rect
		{
			srcSeparationX, 0,
			vscreen->Width - srcSeparationX, vscreen->Height
		};

#if SDL_VERSION_ATLEAST(2, 0, 10)
		// SDL_RenderCopyF was added in 2.0.10
		auto dstSeparationX = DestinationRect.w * tableWidthCoef;
		auto dstBoardRect = SDL_FRect
		{
			DestinationRect.x + offset_x * fullscrn::ScaleX,
			DestinationRect.y + offset_y * fullscrn::ScaleY,
			dstSeparationX, static_cast<float>(DestinationRect.h)
		};
		auto dstSidebarRect = SDL_FRect
		{
			DestinationRect.x + dstSeparationX, static_cast<float>(DestinationRect.y),
			DestinationRect.w - dstSeparationX, static_cast<float>(DestinationRect.h)
		};

		SDL_RenderCopyF(winmain::Renderer, texture, &srcBoardRect, &dstBoardRect);
		SDL_RenderCopyF(winmain::Renderer, texture, &srcSidebarRect, &dstSidebarRect);
#else
		// SDL_RenderCopy cannot express sub pixel offset.
		// Vscreen shift is required for that.
		auto dstSeparationX = static_cast<int>(DestinationRect.w * tableWidthCoef);
		auto scaledOffX = static_cast<int>(round(offset_x * fullscrn::ScaleX));
		if (offset_x != 0 && scaledOffX == 0)
			scaledOffX = Sign(offset_x);
		auto scaledOffY = static_cast<int>(round(offset_y * fullscrn::ScaleY));
		if (offset_y != 0 && scaledOffX == 0)
			scaledOffY = Sign(offset_y);

		auto dstBoardRect = SDL_Rect
		{
			DestinationRect.x + scaledOffX, DestinationRect.y + scaledOffY,
			dstSeparationX, DestinationRect.h
		};
		auto dstSidebarRect = SDL_Rect
		{
			DestinationRect.x + dstSeparationX, DestinationRect.y,
			DestinationRect.w - dstSeparationX, DestinationRect.h
		};

		SDL_RenderCopy(winmain::Renderer, texture, &srcBoardRect, &dstBoardRect);
		SDL_RenderCopy(winmain::Renderer, texture, &srcSidebarRect, &dstSidebarRect);
#endif
	}
}

void render::PresentVScreen()
{
	ApplySidebarTextTints();
	vscreen->BlitToTexture();
	PresentTexture(vscreen->Texture);
	PresentCustomRightPanel();
	PresentSidebarDynamicAreas();

	// Hide only the ball digit during the off phase, without changing its value.
	if (pb::MainTable && pb::MainTable->ScoreBallcount)
	{
		auto ballScore = pb::MainTable->ScoreBallcount;
		if (ballScore->BlinkActive)
		{
			const auto elapsed = SDL_GetTicks() - ballScore->BlinkStartTicks;
			if (elapsed >= 1500u)
				ballScore->BlinkActive = false;
			else if ((elapsed / 150u) % 2u != 0u)
			{
				SDL_Rect source{ballScore->OffsetX, ballScore->OffsetY, ballScore->Width, ballScore->Height};
				auto destination = fullscrn::GetScreenRectFromPinballRect(source);
				Uint8 red, green, blue, alpha;
				SDL_GetRenderDrawColor(winmain::Renderer, &red, &green, &blue, &alpha);
				SDL_SetRenderDrawColor(winmain::Renderer, 0, 0, 0, 255);
				SDL_RenderFillRect(winmain::Renderer, &destination);
				SDL_SetRenderDrawColor(winmain::Renderer, red, green, blue, alpha);
			}
		}
	}

	if (options::Options.DebugOverlay)
	{
		DebugOverlay::DrawOverlay();
	}
}

void render::LoadCustomRightPanel(const std::string& imagePath, const std::string& hdImagePath)
{
	if (customRightPanelTexture)
	{
		SDL_DestroyTexture(customRightPanelTexture);
		customRightPanelTexture = nullptr;
	}

	auto surface = SDL_LoadBMP(imagePath.c_str());
	if (!surface)
	{
		fprintf(stderr, "Custom right panel unavailable: %s\n", SDL_GetError());
		return;
	}
	customRightPanelTexture = SDL_CreateTextureFromSurface(winmain::Renderer, surface);
	SDL_FreeSurface(surface);
	if (!customRightPanelTexture)
		fprintf(stderr, "Unable to create custom right panel texture: %s\n", SDL_GetError());

	auto hdSurface = SDL_LoadBMP(hdImagePath.c_str());
	if (!hdSurface)
	{
		fprintf(stderr, "Custom HD right panel unavailable: %s\n", SDL_GetError());
		return;
	}
	customRightPanelHdTexture = SDL_CreateTextureFromSurface(winmain::Renderer, hdSurface);
	SDL_FreeSurface(hdSurface);
	if (!customRightPanelHdTexture)
		fprintf(stderr, "Unable to create custom HD right panel texture: %s\n", SDL_GetError());
}

void render::PresentCustomRightPanel()
{
	if (!customRightPanelTexture || !vscreen)
		return;

	// Render the original high-resolution artwork only after the low-resolution
	// game canvas has been scaled. This preserves its detail in fullscreen mode.
	static const SDL_Rect panelRects[3]
	{
		{386, 12, 203, 400},
		{482, 15, 252, 496},
		{617, 19, 325, 640}
	};
	const auto resolution = fullscrn::GetResolution();
	const auto logicalRect = panelRects[resolution];
	auto destination = fullscrn::GetScreenRectFromPinballRect(logicalRect);
	SDL_RenderCopy(winmain::Renderer, customRightPanelTexture, nullptr, &destination);

	if (customRightPanelHdTexture)
	{
		// High-resolution replacement for the artwork strictly inside the upper
		// metal frame. The frame itself continues to come from the native panel.
		const SDL_Rect nativeHdRect{620, 22, 319, 304};
		const auto scaleX = logicalRect.w / 325.0f;
		const auto scaleY = logicalRect.h / 640.0f;
		const SDL_Rect logicalHdRect
		{
			logicalRect.x + static_cast<int>(std::lround(3 * scaleX)),
			logicalRect.y + static_cast<int>(std::lround(3 * scaleY)),
			static_cast<int>(std::lround(nativeHdRect.w * scaleX)),
			static_cast<int>(std::lround(nativeHdRect.h * scaleY))
		};
		auto hdDestination = fullscrn::GetScreenRectFromPinballRect(logicalHdRect);
		SDL_RenderCopy(winmain::Renderer, customRightPanelHdTexture, nullptr, &hdDestination);

		// Subtle breathing halo around the title. Reusing only the title band of
		// the HD texture keeps the effect sharp and avoids touching the shuttle.
		constexpr float twoPi = 6.28318530717958647692f;
		const auto seconds = SDL_GetTicks() / 1000.0f;
		const auto titleWave = 0.5f + 0.5f * std::sin(seconds * 2.4f);
		SDL_Rect titleSource{0, 0, 504, 185};
		SDL_Rect titleDestination
		{
			hdDestination.x,
			hdDestination.y,
			hdDestination.w,
			static_cast<int>(std::lround(hdDestination.h * 185.0 / 480.0))
		};
		SDL_SetTextureBlendMode(customRightPanelHdTexture, SDL_BLENDMODE_ADD);
		SDL_SetTextureAlphaMod(customRightPanelHdTexture,
		                       static_cast<uint8_t>(12.0f + titleWave * 58.0f));
		const auto haloOffset = std::max(1, static_cast<int>(std::lround(
			hdDestination.w / 320.0f)));
		for (const auto offset : {SDL_Point{-haloOffset, 0}, SDL_Point{haloOffset, 0},
		                          SDL_Point{0, -haloOffset}, SDL_Point{0, haloOffset}})
		{
			auto haloDestination = titleDestination;
			haloDestination.x += offset.x;
			haloDestination.y += offset.y;
			SDL_RenderCopy(winmain::Renderer, customRightPanelHdTexture,
			               &titleSource, &haloDestination);
		}
		SDL_SetTextureAlphaMod(customRightPanelHdTexture, 255);
		SDL_SetTextureBlendMode(customRightPanelHdTexture, SDL_BLENDMODE_NONE);

		// Pulse four stars already present in the artwork. Their positions are
		// expressed in source-image pixels, so they remain exact at every scale.
		struct AnimatedStar { int X, Y; float Phase; };
		constexpr AnimatedStar stars[]
		{
			{473, 25, 0.0f}, {49, 257, 1.6f},
			{369, 135, 3.2f}, {86, 475, 4.8f}
		};
		SDL_BlendMode oldBlendMode;
		uint8_t oldRed, oldGreen, oldBlue, oldAlpha;
		SDL_GetRenderDrawBlendMode(winmain::Renderer, &oldBlendMode);
		SDL_GetRenderDrawColor(winmain::Renderer, &oldRed, &oldGreen, &oldBlue, &oldAlpha);
		SDL_SetRenderDrawBlendMode(winmain::Renderer, SDL_BLENDMODE_ADD);
		for (const auto& star : stars)
		{
			const auto wave = 0.5f + 0.5f * std::sin(seconds * 4.2f + star.Phase);
			const auto alpha = static_cast<uint8_t>(20.0f + wave * wave * wave * 235.0f);
			const auto x = hdDestination.x + static_cast<int>(std::lround(
				star.X * hdDestination.w / 504.0f));
			const auto y = hdDestination.y + static_cast<int>(std::lround(
				star.Y * hdDestination.h / 480.0f));
			const auto arm = std::max(2, static_cast<int>(std::lround(
				hdDestination.w / 250.0f)));
			SDL_SetRenderDrawColor(winmain::Renderer, 255, 255, 255, alpha);
			SDL_RenderDrawLine(winmain::Renderer, x - arm, y, x + arm, y);
			SDL_RenderDrawLine(winmain::Renderer, x, y - arm, x, y + arm);
		}

		// Six blinking exhaust rays, aligned with the rear of the shuttle in the
		// HD artwork. Warm and red rays use staggered phases for a lively thrust.
		struct ExhaustRay
		{
			int StartX, StartY, EndX, EndY;
			uint8_t Red, Green, Blue;
			float Phase;
		};
		constexpr ExhaustRay exhaustRays[]
		{
			{396, 210, 449, 151, 255, 238, 145, 0.0f},
			{400, 214, 471, 166, 255, 48, 34, 0.8f},
			{403, 219, 490, 183, 255, 238, 145, 1.6f},
			{405, 224, 507, 201, 255, 48, 34, 2.4f},
			{405, 229, 515, 222, 255, 238, 145, 3.2f},
			{403, 234, 519, 246, 255, 48, 34, 4.0f}
		};
		for (const auto& ray : exhaustRays)
		{
			const auto wave = 0.5f + 0.5f * std::sin(seconds * 6.0f + ray.Phase);
			const auto flash = wave * wave;
			const auto alpha = static_cast<uint8_t>(18.0f + flash * 237.0f);
			auto mapX = [&hdDestination](int x)
			{
				return hdDestination.x + static_cast<int>(std::lround(
					x * hdDestination.w / 504.0f));
			};
			auto mapY = [&hdDestination](int y)
			{
				return hdDestination.y + static_cast<int>(std::lround(
					y * hdDestination.h / 480.0f));
			};
			const auto startX = mapX(ray.StartX);
			const auto startY = mapY(ray.StartY);
			const auto endX = mapX(ray.EndX);
			const auto endY = mapY(ray.EndY);
			SDL_SetRenderDrawColor(winmain::Renderer, ray.Red, ray.Green, ray.Blue, alpha);
			SDL_RenderDrawLine(winmain::Renderer, startX, startY, endX, endY);
			if (hdDestination.w >= 500)
				SDL_RenderDrawLine(winmain::Renderer, startX, startY + 1, endX, endY + 1);
		}
		SDL_SetRenderDrawBlendMode(winmain::Renderer, oldBlendMode);
		SDL_SetRenderDrawColor(winmain::Renderer, oldRed, oldGreen, oldBlue, oldAlpha);
	}
}

void render::PresentSidebarDynamicAreas()
{
	if (!customRightPanelTexture || !vscreen || !vscreen->Texture || !pb::MainTable)
		return;

	auto presentArea = [](int x, int y, int width, int height)
	{
		if (width <= 0 || height <= 0)
			return;
		SDL_Rect source{x, y, width, height};
		auto destination = fullscrn::GetScreenRectFromPinballRect(source);
		SDL_RenderCopy(winmain::Renderer, render::vscreen->Texture, &source, &destination);
	};
	auto presentScore = [&presentArea](const scoreStruct* value)
	{
		if (value)
			presentArea(value->OffsetX, value->OffsetY, value->Width, value->Height);
	};

	presentScore(pb::MainTable->CurScoreStruct);
	presentScore(pb::MainTable->ScorePlayerNumber1);
	presentScore(pb::MainTable->ScoreBallcount);
	if (pb::InfoTextBox)
		presentArea(pb::InfoTextBox->OffsetX, pb::InfoTextBox->OffsetY,
		            pb::InfoTextBox->Width, pb::InfoTextBox->Height);
	if (pb::MissTextBox)
		presentArea(pb::MissTextBox->OffsetX, pb::MissTextBox->OffsetY,
		            pb::MissTextBox->Width, pb::MissTextBox->Height);
}

void render::ApplySidebarTextTints()
{
	if (!vscreen || !vscreen->BmpBufPtr1)
		return;

	auto tintPurpleText = [](int x, int y, int width, int height, ColorRgba tint)
	{
		const auto endX = std::min(x + width, render::vscreen->Width);
		const auto endY = std::min(y + height, render::vscreen->Height);
		for (auto py = std::max(y, 0); py < endY; ++py)
		{
			for (auto px = std::max(x, 0); px < endX; ++px)
			{
				auto& pixel = render::vscreen->BmpBufPtr1[py * render::vscreen->Stride + px];
				const auto red = pixel.GetRed();
				const auto green = pixel.GetGreen();
				const auto blue = pixel.GetBlue();
				if (blue <= 60 || blue <= red + 18 || blue <= green + 18)
					continue;
				const auto level = std::max<int>(blue, 128);
				pixel = ColorRgba(
					static_cast<uint8_t>(tint.GetRed() * level / 255),
					static_cast<uint8_t>(tint.GetGreen() * level / 255),
					static_cast<uint8_t>(tint.GetBlue() * level / 255), 255);
			}
		}
	};

	if (pb::MainTable && pb::MainTable->ScoreBallcount)
	{
		const auto score = pb::MainTable->ScoreBallcount;
		tintPurpleText(score->OffsetX, score->OffsetY, score->Width, score->Height,
		               ColorRgba(255, 70, 190, 255));
	}
	if (pb::InfoTextBox)
	{
		// Some long bitmap messages leave partial glyphs in the right-hand padding
		// inside the logical text rectangle. Restore that padding from the original
		// panel after text drawing and before presenting the current frame.
		if (background_bitmap)
		{
			constexpr int desiredStripWidth = 30;
			constexpr int outsideCleanupWidth = 16;
			const auto stripX = std::max(pb::InfoTextBox->OffsetX,
			                             pb::InfoTextBox->OffsetX + pb::InfoTextBox->Width - desiredStripWidth);
			const auto stripWidth = std::min(desiredStripWidth + outsideCleanupWidth, vscreen->Width - stripX);
			if (stripWidth > 0)
				gdrv::copy_bitmap(vscreen, stripWidth, pb::InfoTextBox->Height,
				                  stripX, pb::InfoTextBox->OffsetY, background_bitmap,
				                  stripX, pb::InfoTextBox->OffsetY);
		}
		tintPurpleText(pb::InfoTextBox->OffsetX, pb::InfoTextBox->OffsetY,
		               pb::InfoTextBox->Width, pb::InfoTextBox->Height,
		               ColorRgba(255, 220, 32, 255));
	}
}

void render::CreateSpaceCadetGlow()
{
	if (spaceCadetGlowTexture || !vscreen || !vscreen->BmpBufPtr1)
		return;

	// Restrict extraction to the curved title. This avoids illuminating the
	// spacecraft, the star field and the metal frame around the right panel.
	spaceCadetGlowRect.x = static_cast<int>(vscreen->Width * 0.65f);
	spaceCadetGlowRect.y = static_cast<int>(vscreen->Height * 0.03f);
	spaceCadetGlowRect.w = std::min(static_cast<int>(vscreen->Width * 0.34f),
	                                vscreen->Width - spaceCadetGlowRect.x);
	spaceCadetGlowRect.h = std::min(static_cast<int>(vscreen->Height * 0.205f),
	                                vscreen->Height - spaceCadetGlowRect.y);

	const auto width = spaceCadetGlowRect.w;
	const auto height = spaceCadetGlowRect.h;
	std::vector<uint8_t> letterMask(width * height, 0);
	std::vector<uint8_t> blurMask(width * height, 0);
	std::vector<ColorRgba> pixels(width * height, ColorRgba(0, 0, 0, 0));

	// Extract the warm, bright face of the SPACE CADET letters. The colour
	// conditions deliberately reject the white and blue stars behind them.
	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			// Follow the arch occupied by the title. Colour extraction alone also
			// selects a few warm stars, whereas this curved band isolates the text.
			const auto curve = std::abs(x - width * 0.5f) / (width * 0.5f);
			if (y < 4.0f + curve * 50.0f || y > 72.0f + curve * 58.0f)
				continue;
			const auto& source = vscreen->BmpBufPtr1[
				(spaceCadetGlowRect.y + y) * vscreen->Stride + spaceCadetGlowRect.x + x];
			const auto red = source.GetRed();
			const auto green = source.GetGreen();
			const auto blue = source.GetBlue();
			if (red > 120 && green > 100 && blue > 55 && red > blue + 20 && green > blue + 8)
				letterMask[y * width + x] = static_cast<uint8_t>(std::min(255, (red + green) / 2));
		}
	}

	// Extend the selected bright letter pixels with a distance-weighted dilation.
	// Unlike the former average blur, this remains clearly visible at fullscreen
	// resolutions while retaining soft edges around the original bitmap letters.
	const int radius = std::max(4, width / 65);
	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			int strongest = 0;
			for (int offsetY = -radius; offsetY <= radius; ++offsetY)
			{
				const auto sampleY = y + offsetY;
				if (sampleY < 0 || sampleY >= height)
					continue;
				for (int offsetX = -radius; offsetX <= radius; ++offsetX)
				{
					const auto sampleX = x + offsetX;
					if (sampleX < 0 || sampleX >= width)
						continue;
					const auto distance = static_cast<int>(std::sqrt(
						static_cast<float>(offsetX * offsetX + offsetY * offsetY)));
					if (distance > radius)
						continue;
					strongest = std::max(strongest,
						static_cast<int>(letterMask[sampleY * width + sampleX]) - distance * 32);
				}
			}
			blurMask[y * width + x] = static_cast<uint8_t>(std::max(strongest, 0));
		}
	}

	for (int index = 0; index < width * height; ++index)
	{
		const auto alpha = static_cast<uint8_t>(std::min(255,
			static_cast<int>(blurMask[index]) + static_cast<int>(letterMask[index]) / 2));
		pixels[index] = ColorRgba(255, 235, 175, alpha);
	}

	spaceCadetGlowTexture = SDL_CreateTexture(winmain::Renderer, SDL_PIXELFORMAT_BGRA32,
	                                          SDL_TEXTUREACCESS_STATIC, width, height);
	if (!spaceCadetGlowTexture)
		return;
	SDL_SetTextureBlendMode(spaceCadetGlowTexture, SDL_BLENDMODE_ADD);
	SDL_UpdateTexture(spaceCadetGlowTexture, nullptr, pixels.data(), width * sizeof(ColorRgba));
}

void render::PresentSpaceCadetGlow()
{
	CreateSpaceCadetGlow();
	if (!spaceCadetGlowTexture)
		return;

	constexpr float cycleMilliseconds = 2500.0f;
	const auto phase = static_cast<float>(SDL_GetTicks() % static_cast<uint32_t>(cycleMilliseconds)) /
	                   cycleMilliseconds;
	constexpr float twoPi = 6.28318530717958647692f;
	const auto pulse = 0.5f - 0.5f * std::cos(phase * twoPi);
	SDL_SetTextureAlphaMod(spaceCadetGlowTexture, static_cast<uint8_t>(25.0f + pulse * 230.0f));

	auto destination = fullscrn::GetScreenRectFromPinballRect(spaceCadetGlowRect);
	SDL_RenderCopy(winmain::Renderer, spaceCadetGlowTexture, nullptr, &destination);
}

void render::PresentSidebarStars()
{
	struct Star { int X, Y; uint8_t Red, Green, Blue; };
	constexpr Star colouredStars[]
	{
		{650, 145, 80, 220, 255}, {680, 150, 255, 190, 55},
		{705, 280, 180, 100, 255}, {895, 155, 255, 75, 190},
		{918, 185, 70, 255, 190}, {905, 260, 255, 145, 55},
		{735, 286, 255, 225, 70}, {820, 302, 70, 220, 255},
		{928, 150, 255, 85, 205}
	};
	constexpr Star colouredBlinkingStar{928, 244, 80, 255, 170};
	constexpr Star blinkingStars[]
	{
		{665, 132, 255, 255, 255}, {760, 300, 255, 255, 255},
		{875, 140, 255, 255, 255}, {922, 220, 255, 255, 255}
	};

	auto drawStar = [](const Star& star, uint8_t alpha, int radius)
	{
		SDL_Rect point{star.X, star.Y, 1, 1};
		point = fullscrn::GetScreenRectFromPinballRect(point);
		const auto scale = std::max(1, static_cast<int>(std::round(
			render::DestinationRect.w / static_cast<float>(render::vscreen->Width))));
		const auto arm = std::max(1, radius * scale);
		SDL_SetRenderDrawColor(winmain::Renderer, star.Red, star.Green, star.Blue, alpha);
		SDL_RenderDrawLine(winmain::Renderer, point.x - arm, point.y, point.x + arm, point.y);
		SDL_RenderDrawLine(winmain::Renderer, point.x, point.y - arm, point.x, point.y + arm);
		if (arm >= 2)
		{
			SDL_RenderDrawPoint(winmain::Renderer, point.x - 1, point.y - 1);
			SDL_RenderDrawPoint(winmain::Renderer, point.x + 1, point.y - 1);
			SDL_RenderDrawPoint(winmain::Renderer, point.x - 1, point.y + 1);
			SDL_RenderDrawPoint(winmain::Renderer, point.x + 1, point.y + 1);
		}
	};

	SDL_BlendMode previousBlendMode;
	uint8_t previousRed, previousGreen, previousBlue, previousAlpha;
	SDL_GetRenderDrawBlendMode(winmain::Renderer, &previousBlendMode);
	SDL_GetRenderDrawColor(winmain::Renderer, &previousRed, &previousGreen, &previousBlue, &previousAlpha);
	SDL_SetRenderDrawBlendMode(winmain::Renderer, SDL_BLENDMODE_ADD);
	for (const auto& star : colouredStars)
		drawStar(star, 230, 2);

	const auto time = SDL_GetTicks() / 1000.0f;
	const auto colourWave = 0.5f + 0.5f * std::sin(time * 3.8f);
	drawStar(colouredBlinkingStar, static_cast<uint8_t>(25.0f + colourWave * colourWave * 230.0f), 3);
	for (size_t index = 0; index < sizeof(blinkingStars) / sizeof(blinkingStars[0]); ++index)
	{
		const auto wave = 0.5f + 0.5f * std::sin(time * 4.5f + static_cast<float>(index) * 1.7f);
		const auto flash = wave * wave * wave;
		drawStar(blinkingStars[index], static_cast<uint8_t>(20.0f + flash * 235.0f), 2);
	}
	SDL_SetRenderDrawColor(winmain::Renderer, previousRed, previousGreen, previousBlue, previousAlpha);
	SDL_SetRenderDrawBlendMode(winmain::Renderer, previousBlendMode);
}
