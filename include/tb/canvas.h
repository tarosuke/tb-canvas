// /**********************************************************************
// Canvas
//  * Copyright (C) 2020, 2024 tarosuke<webmaster@tarosuke.net>
//  *
//  * This program is free software; you can redistribute it and/or
//  * modify it under the terms of the GNU General Public License
//  *  as published by the Free Software Foundation; either version 3
//  * of the License, or (at your option) any later version.
//  *
//  * This program is distributed in the hope that it will be useful,
//  * but WITHOUT ANY WARRANTY; without even the implied warranty of
//  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  * GNU General Public License for more details.
//  *
//  * You should have received a copy of the GNU General Public License
//  * along with this program; if not, write to the Free Software
//  * Foundation, Inc.,
//  * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//  */
#pragma once

#include <filesystem>
#include <tb/image.h>
#include <tb/rect.h>
#include <tb/types.h>



extern "C" {
	typedef struct _cairo cairo_t;
	typedef struct _cairo_surface cairo_surface_t;
}


namespace tb {

	/** Canvas
	 * @brief 描画先(いわゆるDrawable)
	 */
	class Canvas {
		Canvas();
		Canvas(const Canvas&);
		void operator=(const Canvas&);

	public:
		/** GC
		 * @brief グラフィックコンテキスト＆描画メソッド
		 */
		class GC {
			GC();
			GC(const GC&);
			void operator=(const GC&);

		public:
			/** Path
			 *  @brief 多角形などのような閉じたパスを作る時にRAIIする
			 */
			class Path {
				Path(const Path&);
				void operator=(const Path&);

			public:
				Path(GC&);
				~Path();

			private:
				cairo_t* const gc;
			};

			enum Slant { slant_normal, slant_italic, slant_oblique };
			enum Weight { weight_normal, weight_bold };
			enum Cap { cap_butt, cap_round, cap_square };
			enum Join { join_miter, join_round, join_bevel };

			GC(Canvas&);
			~GC();

			// 設定
			void SetStroke(Pixel<u8>);
			void SetFill(Pixel<u8>);
			void Set(Pixel<u8> strokeColor, Pixel<u8> fillColor);
			void
			Set(const char* family,
				Slant = slant_normal,
				Weight = weight_normal);
			void SetThickness(double);
			void Set(Cap);
			void Set(Join);

			// 描画
			void Clear(Pixel<u8>);
			void MoveTo(double x, double y);
			void LineTo(double x, double y);
			void
			Arc(double x,
				double y,
				double radius,
				double startAngle,
				double endAngle);
			void CurveTo(
				double x0,
				double y0,
				double xc,
				double yc,
				double x1,
				double y1);
			void Rectandle(double x0, double y0, double x1, double y1);
			void Puts(const char* utf8);

		private:
			cairo_t* gc;
			Canvas& canvas;
			Pixel<u8> strokeColor;
			Pixel<u8> fillColor;
			double thickness;
			Cap cap;
			Join join;
			Rect<2, double> extents;

			void Flush();
		};

		Canvas(unsigned width, unsigned height);
		Canvas(const std::filesystem::path&) noexcept(false);
		virtual ~Canvas();

		struct Image : tb::ImageARGB32 {
			Image(Canvas&);
			~Image();

		private:
			cairo_surface_t* const surface;
		};

	protected:
		cairo_surface_t* const surface;

		virtual void OnCanvasUpdated(const Rect<2, double>&) {};

	private:
		static cairo_surface_t*
		Load(const std::filesystem::path&) noexcept(false);
		static const char* GetExt(const char*);
		static cairo_surface_t* LoadJPEG(const char*);
	};

}
