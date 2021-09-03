#include <vitasdk.h>
#include <vitaGL.h>
#include <imgui_vita.h>
#include <stdio.h>
#include <string>

extern "C" {
void *__wrap_memcpy(void *dest, const void *src, size_t n) {
  return sceClibMemcpy(dest, src, n);
}

void *__wrap_memmove(void *dest, const void *src, size_t n) {
  return sceClibMemmove(dest, src, n);
}

void *__wrap_memset(void *s, int c, size_t n) {
  return sceClibMemset(s, c, n);
}
}

#define CONFIG_FILE_PATH "ux0:data/ff4/options.cfg"

#define RESOLUTION_NUM 3
char *ResolutionName[RESOLUTION_NUM] = {
  "544p",
  "720p",
  "1080i"
};

#define ANTI_ALIASING_NUM 3
char *AntiAliasingName[ANTI_ALIASING_NUM] = {
  "Disabled",
  "MSAA 2x",
  "MSAA 4x"
};

#define LANGUAGES_NUM 12
char *LanguageName[LANGUAGES_NUM] = {
  "Auto",
  "Japanese",
  "English",
  "French",
  "German",
  "Italian",
  "Spanish",
  "Simplified Chinese",
  "Traditional Chinese",
  "Korean",
  "Portuguese",
  "Russian"
};

#define BATTLE_FPS_NUM 4
char *BattleFpsName[BATTLE_FPS_NUM] = {
  "15",
  "20",
  "25",
  "30"
};

typedef struct {
  int res;
  int bilinear;
  int lang;
  int msaa;
  int redub;
  int postfx;
  int battle_fps;
  int debug_menu;
  int swap_confirm;
} config_opts;
config_opts options;

bool bilinear_filter;
bool jap_dub;
bool debug_menu;
bool swap_confirm;

void loadOptions() {
  char buffer[30];
  int value;
    
  FILE *f = fopen(CONFIG_FILE_PATH, "rb");
  if (f) {
    while (EOF != fscanf(f, "%[^=]=%d\n", buffer, &value)) {
      if (strcmp("resolution", buffer) == 0) options.res = value;
      else if (strcmp("bilinear", buffer) == 0) options.bilinear = value;
      else if (strcmp("language", buffer) == 0) options.lang = value;
      else if (strcmp("antialiasing", buffer) == 0) options.msaa = value;
      else if (strcmp("undub", buffer) == 0) options.redub = value;
      else if (strcmp("postfx", buffer) == 0) options.postfx = value;
      else if (strcmp("battle_fps", buffer) == 0) options.battle_fps = value;
      else if (strcmp("debug_menu", buffer) == 0) options.debug_menu = value;
      else if (strcmp("swap_confirm", buffer) == 0) options.swap_confirm = value;
    }
  } else {
    options.res = 0;
    options.bilinear = 0;
    options.lang = 0;
    options.msaa = 2;
    options.redub = 0;
    options.postfx = 0;
    options.battle_fps = 0;
    options.debug_menu = 0;
    options.swap_confirm = 0;
  }
    
  bilinear_filter = options.bilinear ? true : false;
  jap_dub = options.redub ? true : false;
  debug_menu = options.debug_menu ? true : false;
  swap_confirm = options.swap_confirm ? true : false;
}

void saveOptions(void) {
  options.bilinear = bilinear_filter ? 1 : 0;
  options.redub = jap_dub ? 1 : 0;
  options.debug_menu = debug_menu ? 1 : 0;
  options.swap_confirm = swap_confirm ? 1 : 0;

  FILE *config = fopen(CONFIG_FILE_PATH, "w+");

  if (config) {
    fprintf(config, "%s=%d\n", "resolution", options.res);
    fprintf(config, "%s=%d\n", "bilinear", options.bilinear);
    fprintf(config, "%s=%d\n", "language", options.lang);
    fprintf(config, "%s=%d\n", "antialiasing", options.msaa);
    fprintf(config, "%s=%d\n", "undub", options.redub);
    fprintf(config, "%s=%d\n", "postfx", options.postfx);
    fprintf(config, "%s=%d\n", "battle_fps", options.battle_fps);
    fprintf(config, "%s=%d\n", "debug_menu", options.debug_menu);
    fprintf(config, "%s=%d\n", "swap_confirm", options.swap_confirm);
    fclose(config);
  }
}

char *options_descs[] = {
  "Internal resolution to use. When a resolution higher than 544p is used on a PSVita, Sharpscale plugin is required.\nThe default value is: 544p.", // resolution
  "When enabled, forces bilinear filtering for all game's textures.\nThe default value is: Disabled.", // bilinear
  "Anti-Aliasing is a technique used to reduce graphical artifacts surrounding 3D models. Greatly improves graphics quality at the cost of some GPU power.\nThe default value is: MSAA 4x.", // antialiasing
  "Language to use for the game. When Auto is used, language will be decided based on system language.\nThe default value is: Auto.", // language
  "When enabled, original japanese dub will be used for cutscenes.\nThe default value is: Disabled.", // undub
  "Enables usage of a post processing effect through shaders. May impact performances.\nThe default value is: Disabled.", // postfx
  "Alters the game framerate during battles.\nThe default value is: 15.", // battle_fps
  "When enabled, internal development debug menu is accessible by pressing SELECT + UP/DOWN.\nThe default value is: Disabled.", // debug_menu
  "When enabled, functionalities of X and O buttons are swapped.\nThe default value is: Disabled." // swap_confirm
};

enum {
  OPT_RESOLUTION,
  OPT_BILINEAR,
  OPT_ANTIALIASING,
  OPT_LANGUAGE,
  OPT_UNDUB,
  OPT_POSTFX,
  OPT_BATTLE_FPS,
  OPT_DEBUG_MENU,
  OPT_CONFIRM_SWAP
};

char *desc = nullptr;

void SetDescription(int i) {
  if (ImGui::IsItemHovered())
    desc = options_descs[i];
}

int main(int argc, char *argv[]) {
  loadOptions();
  int exit_code = 0xDEAD;

  vglInitExtended(0, 960, 544, 0x1800000, SCE_GXM_MULTISAMPLE_4X);
  ImGui::CreateContext();
  ImGui_ImplVitaGL_Init();
  ImGui_ImplVitaGL_TouchUsage(false);
  ImGui_ImplVitaGL_GamepadUsage(true);
  ImGui::StyleColorsDark();
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

  ImGui::GetIO().MouseDrawCursor = false;
  
  // Generating PostFX array
  char none_str[5];
  strcpy(none_str, "None");
  char *PostFxName[64] = {0};
  PostFxName[0] = none_str;
  int PostFxNum = 1;
  SceIoDirent d;
  SceUID fd = sceIoDopen("app0:shaders");
  while (sceIoDread(fd, &d) > 0) {
    int n;
    char name[64];
    sscanf(d.d_name, "%d_%s", &n, name);
    strcpy(name, strchr(d.d_name, '_') + 1);
    char *end_of_name = strchr(name, '_');
    end_of_name[0] = 0;
    if (PostFxName[n] == NULL) {
      PostFxName[n] = (char *)malloc(strlen(name) + 1);
      strcpy(PostFxName[n], name);
      if (PostFxNum <= n)
        PostFxNum = n + 1;
    }
  }
  sceIoDclose(fd);

  while (exit_code == 0xDEAD) {
    desc = nullptr;
    ImGui_ImplVitaGL_NewFrame();

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiSetCond_Always);
    ImGui::SetNextWindowSize(ImVec2(960, 544), ImGuiSetCond_Always);
    ImGui::Begin("##main", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImGui::TextColored(ImVec4(255, 255, 0, 255), "Graphics");
  
    ImGui::Text("Resolution:"); ImGui::SameLine();
    if (ImGui::BeginCombo("##combo0", ResolutionName[options.res])) {
      for (int n = 0; n < RESOLUTION_NUM; n++) {
        bool is_selected = options.res == n;
        if (ImGui::Selectable(ResolutionName[n], is_selected))
          options.res = n;
        SetDescription(OPT_RESOLUTION);
        if (is_selected)
          ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
    }
    SetDescription(OPT_RESOLUTION);
  
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
    ImGui::Text("Bilinear Filter:"); ImGui::SameLine();
    ImGui::Checkbox("##check0", &bilinear_filter);
    SetDescription(OPT_BILINEAR);
    ImGui::PopStyleVar();
    
    ImGui::Text("Anti-Aliasing:"); ImGui::SameLine();
    if (ImGui::BeginCombo("##combo1", AntiAliasingName[options.msaa])) {
      for (int n = 0; n < ANTI_ALIASING_NUM; n++) {
        bool is_selected = options.msaa == n;
        if (ImGui::Selectable(AntiAliasingName[n], is_selected))
          options.msaa = n;
        SetDescription(OPT_ANTIALIASING);
        if (is_selected)
          ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
    }
    SetDescription(OPT_ANTIALIASING);

    ImGui::Text("PostFX Effect:"); ImGui::SameLine();
    if (ImGui::BeginCombo("##combo2", PostFxName[options.postfx])) {
      for (int n = 0; n < PostFxNum; n++) {
        bool is_selected = options.postfx == n;
        if (ImGui::Selectable(PostFxName[n], is_selected))
          options.postfx = n;
        SetDescription(OPT_POSTFX);
        if (is_selected)
          ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
    }
    SetDescription(OPT_POSTFX);

    ImGui::Separator();
    ImGui::TextColored(ImVec4(255, 255, 0, 255), "Misc");

    ImGui::Text("Language:"); ImGui::SameLine();
    if (ImGui::BeginCombo("##combo3", LanguageName[options.lang])) {
      for (int n = 0; n < LANGUAGES_NUM; n++) {
        bool is_selected = options.lang == n;
        if (ImGui::Selectable(LanguageName[n], is_selected))
          options.lang = n;
        SetDescription(OPT_LANGUAGE);
        if (is_selected)
          ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
    }
    SetDescription(OPT_LANGUAGE);
    
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
    ImGui::Text("Japanese Dub:"); ImGui::SameLine();
    ImGui::Checkbox("##check1", &jap_dub);
    SetDescription(OPT_UNDUB);
    ImGui::PopStyleVar();

    ImGui::Text("FPS in Battle:"); ImGui::SameLine();
    if (ImGui::BeginCombo("##combo4", BattleFpsName[options.battle_fps])) {
      for (int n = 0; n < BATTLE_FPS_NUM; n++) {
        bool is_selected = options.battle_fps == n;
        if (ImGui::Selectable(BattleFpsName[n], is_selected))
          options.battle_fps = n;
        SetDescription(OPT_BATTLE_FPS);
        if (is_selected)
          ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
    }
    SetDescription(OPT_BATTLE_FPS);

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
    ImGui::Text("Debug Menu:"); ImGui::SameLine();
    ImGui::Checkbox("##check2", &debug_menu);
    SetDescription(OPT_DEBUG_MENU);

    ImGui::Text("Swap Confirm Button:"); ImGui::SameLine();
    ImGui::Checkbox("##check3", &swap_confirm);
    SetDescription(OPT_CONFIRM_SWAP);
    ImGui::PopStyleVar();
    
    ImGui::Separator();

    if (ImGui::Button("Save and Exit"))
      exit_code = 0;
    ImGui::SameLine();
    if (ImGui::Button("Save and Launch the game"))
      exit_code = 1;
    ImGui::SameLine();
    if (ImGui::Button("Discard and Exit"))
      exit_code = 2;
    ImGui::SameLine();
    if (ImGui::Button("Discard and Launch the game"))
      exit_code = 3;
    ImGui::Separator();

    if (desc) {
      ImGui::Spacing();
      ImGui::Spacing();
      ImGui::TextWrapped(desc);
    }
    ImGui::End();
  
    glViewport(0, 0, static_cast<int>(ImGui::GetIO().DisplaySize.x), static_cast<int>(ImGui::GetIO().DisplaySize.y));
    ImGui::Render();
    ImGui_ImplVitaGL_RenderDrawData(ImGui::GetDrawData());
    vglSwapBuffers(GL_FALSE);
  }

  if (exit_code < 2) // Save
    saveOptions();

  if (exit_code % 2 == 1) // Launch
    sceAppMgrLoadExec("app0:/eboot.bin", NULL, NULL);

  return 0;
}
