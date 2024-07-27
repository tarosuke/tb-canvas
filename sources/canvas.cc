// /**********************************************************************
// Canvas
//  * Copyright (C) 2020, 2924 tarosuke<webmaster@tarosuke.net>
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

#include <cairo/cairo.h>
#include <stdio.h>
#include <tb/canvas.h>

#include <fcntl.h>
#include <jpeglib.h>
#include <memory.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <syslog.h>
#include <unistd.h>



namespace tb {

	Canvas::Image::Image(Canvas& canvas)
		: tb::Image<tb::Pixel<u8>>(
			  cairo_image_surface_get_data(canvas.surface),
			  cairo_image_surface_get_width(canvas.surface),
			  cairo_image_surface_get_height(canvas.surface),
			  cairo_image_surface_get_stride(canvas.surface)),
		  surface(canvas.surface) {}
	Canvas::Image::~Image() { cairo_surface_mark_dirty(surface); }



	Canvas::GC::GC(Canvas& c) : gc(cairo_create(c.surface)), canvas(c) {}

	Canvas::GC::~GC() {
		Flush();
		cairo_destroy(gc);
		canvas.OnCanvasUpdated(extents);
	}

	void Canvas::GC::Flush() {
		static const cairo_line_join_t joins[] = {
			CAIRO_LINE_JOIN_MITER,
			CAIRO_LINE_JOIN_ROUND,
			CAIRO_LINE_JOIN_BEVEL,
		};
		static const cairo_line_cap_t caps[] = {
			CAIRO_LINE_CAP_BUTT,
			CAIRO_LINE_CAP_ROUND,
			CAIRO_LINE_CAP_SQUARE,
		};

		// 更新範囲を更新
		double ex[4];
		cairo_stroke_extents(gc, &ex[0], &ex[1], &ex[2], &ex[3]);
		const Rect<2, double> e(
			Vector<2, double>({ex[0], ex[1]}),
			Vector<2, double>({ex[2], ex[3]}));

		extents |= e;

		// フィル描画
		cairo_set_source_rgba(
			gc,
			fillColor[1],
			fillColor[2],
			fillColor[3],
			fillColor[0]);
		cairo_fill_preserve(gc);

		// ストローク描画
		cairo_set_source_rgba(
			gc,
			strokeColor[1],
			strokeColor[2],
			strokeColor[3],
			strokeColor[0]);
		cairo_set_line_width(gc, thickness);
		cairo_set_line_cap(gc, caps[cap]);
		cairo_set_line_join(gc, joins[join]);
		cairo_stroke(gc);
	}

	void Canvas::GC::SetStroke(Pixel<u8> c) {
		Flush();
		strokeColor = c;
	}
	void Canvas::GC::SetFill(Pixel<u8> c) {
		Flush();
		fillColor = c;
	}
	void Canvas::GC::Set(Pixel<u8> sc, Pixel<u8> fc) {
		Flush();
		strokeColor = sc;
		fillColor = fc;
	}
	void Canvas::GC::Set(const char* family, Slant slant, Weight weight) {
		static const cairo_font_slant_t slants[] = {
			CAIRO_FONT_SLANT_NORMAL,
			CAIRO_FONT_SLANT_ITALIC,
			CAIRO_FONT_SLANT_OBLIQUE,
		};
		static const cairo_font_weight_t weights[] = {
			CAIRO_FONT_WEIGHT_NORMAL,
			CAIRO_FONT_WEIGHT_BOLD,
		};
		Flush();
		cairo_select_font_face(gc, family, slants[slant], weights[weight]);
	}
	void Canvas::GC::SetThickness(double t) {
		Flush();
		thickness = t;
	}
	void Canvas::GC::Set(Cap c) {
		Flush();
		cap = c;
	}
	void Canvas::GC::Set(Join j) {
		Flush();
		join = j;
	}


	void Canvas::GC::Clear(Pixel<u8> c) {
		cairo_pattern_t* const p(cairo_get_source(gc));
		cairo_set_source_rgba(gc, c[1], c[2], c[3], c[0]);
		cairo_paint(gc);
		cairo_set_source(gc, p);
	}
	void Canvas::GC::MoveTo(double x, double y) { cairo_move_to(gc, x, y); }
	void Canvas::GC::LineTo(double x, double y) { cairo_line_to(gc, x, y); }
	void Canvas::GC::Arc(
		double x, double y, double radius, double startAngle, double endAngle) {
		cairo_arc(gc, x, y, radius, startAngle, endAngle);
	}
	void Canvas::GC::CurveTo(
		double x0, double y0, double xc, double yc, double x1, double y1) {
		cairo_curve_to(gc, x0, y0, xc, yc, x1, y1);
	}
	void Canvas::GC::Rectandle(double x0, double y0, double x1, double y1) {
		cairo_rectangle(gc, x0, y0, x1, y1);
	}
	void Canvas::GC::Puts(const char* utf8) { cairo_show_text(gc, utf8); }



	Canvas::GC::Path::Path(GC& gc) : gc(gc.gc) {
		gc.Flush();
		cairo_new_path(gc.gc);
	}

	Canvas::GC::Path::~Path() { cairo_close_path(gc); }


	Canvas::Canvas(unsigned width, unsigned height) noexcept(false)
		: surface(
			  cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height)) {}

	Canvas::Canvas(const std::filesystem::path& path) : surface(Load(path)) {}

	Canvas::~Canvas() {
		if (surface) {
			cairo_surface_destroy(surface);
		}
	}

	cairo_surface_t*
	Canvas::Load(const std::filesystem::path& path) noexcept(false) {
		static const struct EXTHandler {
			const char* const ext;
			cairo_surface_t* (*const loader)(const char*);
		} exts[] = {{".jpg", LoadJPEG}, {".jpeg", LoadJPEG}, {}};

		const auto ext(path.extension());
		for (const EXTHandler* l(exts); (*l).ext; ++l) {
			if (!strcasecmp((*l).ext, ext.c_str())) {
				if (auto* const s = (*l).loader(path.c_str())) {
					return s;
				}
			}
		}
		if (auto* s = cairo_image_surface_create_from_png(path.c_str())) {
			return s;
		}
		throw -1;
	}

	static jmp_buf env;
	static void HandleError(j_common_ptr cinfo) { longjmp(env, 0); }
	cairo_surface_t* Canvas::LoadJPEG(const char* path) {
		cairo_surface_t* surface(0);

		// ファイルを開いておく
		const int fd(open(path, O_RDONLY));
		if (fd < 0) {
			return 0;
		}; // 失敗

		// ファイルをまるごとメモリへマップ
		struct stat stat;
		fstat(fd, &stat);
		unsigned char* const jpeg(
			(unsigned char*)
				mmap(NULL, stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0));
		if (!jpeg) {
			return 0; // mmap失敗
		}

		struct jpeg_decompress_struct ci;
		struct jpeg_error_mgr jerr;
		JSAMPARRAY jarr(0);

		// 伸張準備
		ci.err = jpeg_std_error(&jerr);
		jpeg_create_decompress(&ci);

		// 元データを設定
		jpeg_mem_src(&ci, jpeg, stat.st_size);

		try {
			// エラーリターン設定
			if (setjmp(env)) {
				throw 0;
			}

			// エラーハンドラ設定
			jerr.error_exit = HandleError;

			// ヘッダ取得
			jpeg_read_header(&ci, TRUE);
			ci.out_color_space = JCS_EXT_BGRX; // そして色空間設定

			// 画像確保
			if (!(surface = cairo_image_surface_create(
					  CAIRO_FORMAT_RGB24,
					  ci.image_width,
					  ci.image_height))) {
				throw 0; // 格納先確保失敗
			}

			// 行配列割り当て
			const unsigned bpl(ci.image_width * 4);
			unsigned char* const buff(cairo_image_surface_get_data(surface));
			jarr = new JSAMPROW[ci.image_height];
			if (!jarr) {
				throw 0;
			}
			for (unsigned n(0); n < ci.image_height; ++n) {
				jarr[n] = (JSAMPROW)&buff[bpl * n];
			}

			// 展開開始
			jpeg_start_decompress(&ci);
			while (ci.output_scanline < ci.output_height) {
				jpeg_read_scanlines(
					&ci,
					&jarr[ci.output_scanline],
					ci.output_height - ci.output_scanline);
			}

			// 終了
			jpeg_finish_decompress(&ci);
		} catch (...) {
			if (surface) {
				cairo_surface_destroy(surface);
				surface = 0;
			}
		}
		jpeg_destroy_decompress(&ci);
		if (jarr) {
			delete jarr;
		}
		munmap(jpeg, stat.st_size);
		close(fd);

		return surface;
	}
}
