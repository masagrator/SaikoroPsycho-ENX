#include "lib.hpp"
#include "ENG.hpp"
#include "xxHash/xxhash.h"
#include "textureHashes.hpp"
#include "nn/fs.hpp"
#include "spng/spng.h"

//It must match character count of ENG::CLEAR_PASSWORD string + 1
char16_t PASSWORD_ENTER[20] = u"";

ptrdiff_t returnInstructionOffset(uintptr_t LR) {
	return LR - exl::util::modules::GetTargetOffset(0);
}

enum XTX_TYPE {
	XTX_BC3_1    = 2,
	XTX_BC3_2    = 3,
	XTX_RGBA4444 = 5,
	XTX_RGBA8    = 8,
	XTX_ASTC_4X4 = 9
};

struct XTX_HEADER {
	const char MAGIC[4];
	uint8_t xtx_type;
	bool isLittleEndian;
	char reserved[2];
	uint32_t texture_width;
	uint32_t texture_height;
	uint32_t image_width;
	uint32_t image_height;
	uint32_t offset_X;
	uint32_t offset_Y;
};

struct XTX_FILE {
	XTX_HEADER header;
	char texture_start[2];
};

XTX_FILE* ReplaceXTXTexture(XTX_FILE* TexturePointer, const char* index) {

		nn::fs::FileHandle filehandle;
		char filepath[128] = "";
		snprintf(&filepath[0], 128, "rom:/%s.png", index);
		if (R_FAILED(nn::fs::OpenFile(&filehandle, filepath, nn::fs::OpenMode_Read))) {
			return nullptr;
		}
		long in_size = 0;
		nn::fs::GetFileSize(&in_size, filehandle);
		void* in_buffer = malloc(in_size);
		nn::fs::ReadFile(filehandle, 0, in_buffer, in_size);
		nn::fs::CloseFile(filehandle);
		spng_ctx *ctx = spng_ctx_new(0);
		spng_set_png_buffer(ctx, in_buffer, in_size);
		size_t out_size = 0;
		spng_decoded_image_size(ctx, SPNG_FMT_RGBA8, &out_size);
		XTX_FILE* out_buffer = (XTX_FILE*)malloc(out_size+sizeof(XTX_HEADER));
		memcpy((void*)out_buffer, TexturePointer, sizeof(XTX_HEADER));
		spng_decode_image(ctx, out_buffer -> texture_start, out_size, SPNG_FMT_RGBA8, 0);
		spng_ctx_free(ctx);
		free(in_buffer);
		out_buffer -> header.xtx_type = XTX_RGBA8;
		return out_buffer;
}

HOOK_DEFINE_TRAMPOLINE(LoadXTXTexture) {
	static int Callback(void* GLTextures, XTX_FILE* TexturePointer) {
		// Sometimes this function is called to just clean textures, 
		// then TexturePointer leads to empty space
		if (!strncmp(TexturePointer -> header.MAGIC, "xtx", 3)) {
			if (TexturePointer -> header.offset_X || TexturePointer -> header.offset_Y) {
				//Those offsets are used only for diff textures overlayed over base image, 
				//there is no good reason to support such textures
				return Orig(GLTextures, TexturePointer);
			}
			uint32_t width = TexturePointer -> header.texture_width;
			uint32_t height = TexturePointer -> header.texture_height;
			if (TexturePointer -> header.isLittleEndian == false) {
				width = __builtin_bswap32(width);
				height = __builtin_bswap32(height);
			}
			//case 9, 2 & 3
			uint32_t xtx_size = width * height;
			switch(TexturePointer -> header.xtx_type) {
				case XTX_RGBA8:
					xtx_size *= 4;
					break;
				case XTX_RGBA4444:
					xtx_size *= 2;
			}
			xtx_size += sizeof(XTX_HEADER);
			XXH64_hash_t hash_output = XXH3_64bits((void*)TexturePointer, xtx_size);
			const char* index = compareHashes(hash_output);
			if (index != nullptr) {
				XTX_FILE* TexturePointer_temp = ReplaceXTXTexture(TexturePointer, index);
				if (TexturePointer_temp) {
					int ret = Orig(GLTextures, TexturePointer_temp);
					free(TexturePointer_temp);
					return ret;
				}
			}
		}

		return Orig(GLTextures, TexturePointer);
	}
};

struct nnswkbdKeyboardConfig {
	uint8_t mode;
};

extern "C" {
	int _ZN2nn5swkbd12ShowKeyboardEPNS0_6StringERKNS0_15ShowKeyboardArgE(const char* string, nnswkbdKeyboardConfig* config);
}

HOOK_DEFINE_TRAMPOLINE(ShowKeyboard) {
	static int Callback(const char* string, nnswkbdKeyboardConfig* config) {
		//Block swkbd to use only ASCII, sadly it doesn't support spaces
		config -> mode = 2;
		return Orig(string, config);
	}
};

extern "C" void exl_main(void* x0, void* x1) {
	*(uint64_t*)exl::util::modules::GetTargetOffset(0x18DA80) = (uint64_t)&ENG::NOTIFICATIONS;
	*(uint64_t*)exl::util::modules::GetTargetOffset(0x18D2E8) = (uint64_t)&ENG::PLAYING_LOG_MAIN;
	*(uint64_t*)exl::util::modules::GetTargetOffset(0x18D718) = (uint64_t)&ENG::SETTINGS_DESCRIPTIONS;
	*(uint64_t*)exl::util::modules::GetTargetOffset(0x18D2E0) = (uint64_t)&ENG::CHAPTERS;
	*(uint64_t*)exl::util::modules::GetTargetOffset(0x18D6E8) = (uint64_t)&PASSWORD_ENTER;
	*(uint64_t*)exl::util::modules::GetTargetOffset(0x18D300) = (uint64_t)&ENG::ENDINGS;

	exl::hook::Initialize();

	LoadXTXTexture::InstallAtOffset(0x12F40);
	ShowKeyboard::InstallAtFuncPtr(_ZN2nn5swkbd12ShowKeyboardEPNS0_6StringERKNS0_15ShowKeyboardArgE);

	exl::patch::CodePatcher patcher{0x3D9F0};
	uint64_t save_text_address = (uint64_t)exl::util::modules::GetTargetOffset(0x3D9F0) & ~0xFFF;
	ptrdiff_t save_text0 = (uint64_t)ENG::SAVE_EXIT[0] - save_text_address;
	ptrdiff_t save_text1 = (uint64_t)ENG::SAVE_EXIT[1] - save_text_address;
	ptrdiff_t save_text2 = (uint64_t)ENG::SAVE_EXIT[2] - save_text_address;
	patcher.WriteInst(exl::armv8::inst::Adrp(exl::armv8::reg::X3, save_text0 & ~0xFFF));
	patcher.WriteInst(exl::armv8::inst::Adrp(exl::armv8::reg::X4, save_text1 & ~0xFFF));
	patcher.WriteInst(exl::armv8::inst::Adrp(exl::armv8::reg::X5, save_text2 & ~0xFFF));
	patcher.WriteInst(exl::armv8::inst::AddImmediate(exl::armv8::reg::X3, exl::armv8::reg::X3, save_text0 & 0xFFF));
	patcher.WriteInst(exl::armv8::inst::AddImmediate(exl::armv8::reg::X4, exl::armv8::reg::X4, save_text1 & 0xFFF));
	patcher.WriteInst(exl::armv8::inst::AddImmediate(exl::armv8::reg::X5, exl::armv8::reg::X5, save_text2 & 0xFFF));

	patcher.Seek(0x17F7C);
	save_text_address = (uint64_t)exl::util::modules::GetTargetOffset(0x17F7C) & ~0xFFF;
	save_text0 = (uint64_t)ENG::YES_NO[0] - save_text_address;
	patcher.WriteInst(exl::armv8::inst::Adrp(exl::armv8::reg::X24, save_text0 & ~0xFFF));
	patcher.WriteInst(exl::armv8::inst::AddImmediate(exl::armv8::reg::X24, exl::armv8::reg::X24, save_text0 & 0xFFF));
	patcher.Seek(0x17E04);
	patcher.WriteInst(exl::armv8::inst::Adrp(exl::armv8::reg::X23, save_text0 & ~0xFFF));
	patcher.WriteInst(exl::armv8::inst::AddImmediate(exl::armv8::reg::X23, exl::armv8::reg::X23, save_text0 & 0xFFF));

	patcher.Seek(0x18034);
	save_text_address = (uint64_t)exl::util::modules::GetTargetOffset(0x18034) & ~0xFFF;
	save_text0 = (uint64_t)ENG::YES_NO[1] - save_text_address;
	patcher.WriteInst(exl::armv8::inst::Adrp(exl::armv8::reg::X0, save_text0 & ~0xFFF));
	patcher.WriteInst(exl::armv8::inst::AddImmediate(exl::armv8::reg::X0, exl::armv8::reg::X0, save_text0 & 0xFFF));

	patcher.Seek(0x17DB4);
	save_text_address = (uint64_t)exl::util::modules::GetTargetOffset(0x17DB4) & ~0xFFF;
	save_text0 = (uint64_t)ENG::YES_NO[2] - save_text_address;
	patcher.WriteInst(exl::armv8::inst::Adrp(exl::armv8::reg::X0, save_text0 & ~0xFFF));
	patcher.WriteInst(exl::armv8::inst::AddImmediate(exl::armv8::reg::X0, exl::armv8::reg::X0, save_text0 & 0xFFF));
	patcher.Seek(0x17F0C);
	patcher.WriteInst(exl::armv8::inst::Adrp(exl::armv8::reg::X0, save_text0 & ~0xFFF));
	patcher.WriteInst(exl::armv8::inst::AddImmediate(exl::armv8::reg::X0, exl::armv8::reg::X0, save_text0 & 0xFFF));

	patcher.Seek(0x17EC4);
	save_text_address = (uint64_t)exl::util::modules::GetTargetOffset(0x17EC4) & ~0xFFF;
	save_text0 = (uint64_t)ENG::YES_NO[3] - save_text_address;
	patcher.WriteInst(exl::armv8::inst::Adrp(exl::armv8::reg::X0, save_text0 & ~0xFFF));
	patcher.WriteInst(exl::armv8::inst::AddImmediate(exl::armv8::reg::X0, exl::armv8::reg::X0, save_text0 & 0xFFF));
	patcher.Seek(0x1801C);
	save_text_address = (uint64_t)exl::util::modules::GetTargetOffset(0x1801C) & ~0xFFF;
	save_text0 = (uint64_t)ENG::YES_NO[3] - save_text_address;
	patcher.WriteInst(exl::armv8::inst::Adrp(exl::armv8::reg::X0, save_text0 & ~0xFFF));
	patcher.WriteInst(exl::armv8::inst::AddImmediate(exl::armv8::reg::X0, exl::armv8::reg::X0, save_text0 & 0xFFF));

	patcher.Seek(0x3C324);
	save_text_address = (uint64_t)exl::util::modules::GetTargetOffset(0x3C324) & ~0xFFF;
	save_text0 = (uint64_t)ENG::QUICK_SAVE[0] - save_text_address;
	save_text1 = (uint64_t)ENG::QUICK_SAVE[1] - save_text_address;
	patcher.WriteInst(exl::armv8::inst::Adrp(exl::armv8::reg::X3, save_text0 & ~0xFFF));
	patcher.WriteInst(exl::armv8::inst::Adrp(exl::armv8::reg::X4, save_text1 & ~0xFFF));
	patcher.WriteInst(exl::armv8::inst::AddImmediate(exl::armv8::reg::X3, exl::armv8::reg::X3, save_text0 & 0xFFF));
	patcher.WriteInst(exl::armv8::inst::AddImmediate(exl::armv8::reg::X4, exl::armv8::reg::X4, save_text1 & 0xFFF));

	patcher.Seek(0x1D1C4);
	save_text_address = (uint64_t)exl::util::modules::GetTargetOffset(0x1D1C4) & ~0xFFF;
	save_text0 = (uint64_t)&ENG::SAVING[0] - save_text_address;
	patcher.WriteInst(exl::armv8::inst::Adrp(exl::armv8::reg::X20, save_text0 & ~0xFFF));
	patcher.WriteInst(exl::armv8::inst::AddImmediate(exl::armv8::reg::X20, exl::armv8::reg::X20, save_text0 & 0xFFF));

	patcher.Seek(0x207C4);
	save_text_address = (uint64_t)exl::util::modules::GetTargetOffset(0x207C4) & ~0xFFF;
	save_text0 = (uint64_t)&ENG::SAVING[0] - save_text_address;
	patcher.WriteInst(exl::armv8::inst::Adrp(exl::armv8::reg::X20, save_text0 & ~0xFFF));
	patcher.WriteInst(exl::armv8::inst::AddImmediate(exl::armv8::reg::X20, exl::armv8::reg::X20, save_text0 & 0xFFF));

	patcher.Seek(0xF774);
	save_text_address = (uint64_t)exl::util::modules::GetTargetOffset(0xF774) & ~0xFFF;
	save_text0 = (uint64_t)ENG::PRISON[0] - save_text_address;
	save_text1 = (uint64_t)ENG::PRISON[1] - save_text_address;
	patcher.WriteInst(exl::armv8::inst::Adrp(exl::armv8::reg::X1, save_text0 & ~0xFFF));
	patcher.WriteInst(exl::armv8::inst::AddImmediate(exl::armv8::reg::X1, exl::armv8::reg::X1, save_text0 & 0xFFF));
	patcher.WriteInst(exl::armv8::inst::Adrp(exl::armv8::reg::X2, save_text1 & ~0xFFF));
	patcher.WriteInst(exl::armv8::inst::AddImmediate(exl::armv8::reg::X2, exl::armv8::reg::X2, save_text1 & 0xFFF));

	patcher.Seek(0x3C12C);
	save_text_address = (uint64_t)exl::util::modules::GetTargetOffset(0x3C12C) & ~0xFFF;
	save_text0 = (uint64_t)&ENG::BACKLOG_JUMP[0] - save_text_address;
	patcher.WriteInst(exl::armv8::inst::Adrp(exl::armv8::reg::X0, save_text0 & ~0xFFF));
	patcher.WriteInst(exl::armv8::inst::AddImmediate(exl::armv8::reg::X0, exl::armv8::reg::X0, save_text0 & 0xFFF));

	patcher.Seek(0xF840);
	save_text_address = (uint64_t)exl::util::modules::GetTargetOffset(0xF840) & ~0xFFF;
	save_text0 = (uint64_t)&ENG::CLEAR_PASSWORD[0] - save_text_address;
	patcher.WriteInst(exl::armv8::inst::Adrp(exl::armv8::reg::X1, save_text0 & ~0xFFF));
	patcher.WriteInst(exl::armv8::inst::AddImmediate(exl::armv8::reg::X1, exl::armv8::reg::X1, save_text0 & 0xFFF));
	patcher.WriteInst(exl::armv8::inst::Movz(exl::armv8::reg::W2, std::char_traits<char16_t>::length(ENG::CLEAR_PASSWORD)));
	patcher.Seek(0xF0E8);
	patcher.WriteInst(exl::armv8::inst::Movz(exl::armv8::reg::X9, std::char_traits<char16_t>::length(ENG::CLEAR_PASSWORD)));

	//Patch password auto-fill to copy translated password
	patcher.Seek(0xF7E0);
	save_text_address = (uint64_t)exl::util::modules::GetTargetOffset(0xF7E0) & ~0xFFF;
	save_text0 = (uint64_t)&ENG::CLEAR_PASSWORD[0] - save_text_address;
	save_text1 = (uint64_t)&PASSWORD_ENTER[0] - save_text_address;
	patcher.WriteInst(exl::armv8::inst::Adrp(exl::armv8::reg::X1, save_text0 & ~0xFFF));
	patcher.WriteInst(exl::armv8::inst::AddImmediate(exl::armv8::reg::X1, exl::armv8::reg::X1, save_text0 & 0xFFF));
	patcher.WriteInst(exl::armv8::inst::Adrp(exl::armv8::reg::X0, save_text1 & ~0xFFF));
	patcher.WriteInst(exl::armv8::inst::AddImmediate(exl::armv8::reg::X0, exl::armv8::reg::X0, save_text1 & 0xFFF));
	patcher.WriteInst(exl::armv8::inst::Movz(exl::armv8::reg::W2, (std::char_traits<char16_t>::length(ENG::CLEAR_PASSWORD) + 1) * 2));
	patcher.WriteInst(0x94035023);
	patcher.WriteInst(exl::armv8::inst::Nop());	
	patcher.WriteInst(exl::armv8::inst::Nop());	
	patcher.WriteInst(exl::armv8::inst::Nop());	


	//Remove Clear Password callback to not block it to katakana only
	patcher.Seek(0xF188);
	patcher.WriteInst(exl::armv8::inst::Nop());	

	//Remove forward slash used for YES/NO
	patcher.Seek(0x12E9A7);
	patcher.WriteInst(0x0);
}

extern "C" NORETURN void exl_exception_entry() {
	/* TODO: exception handling */
	EXL_ABORT(0x420);
}