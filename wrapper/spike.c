/* Milestone 1 gating spike: can we dlopen the Bionic-built libdrastic_arm64.so
 * on aarch64 musl? Prints per-symbol resolution for every exported JNI entry
 * point the libretro wrapper needs, without running the emulator.
 */
#include <stdio.h>
#include <dlfcn.h>

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "usage: %s <path-to-libdrastic_arm64.so>\n", argv[0]);
		return 2;
	}
	const char *path = argv[1];

	void *h = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
	if (!h) {
		fprintf(stderr, "DLOPEN FAILED: %s\n", dlerror());
		return 1;
	}
	printf("dlopen OK: %s\n", path);

	static const char *syms[] = {
		"JNI_OnLoad",
		"Java_com_dsemu_drastic_DraSticJNI_onInit",
		"Java_com_dsemu_drastic_DraSticJNI_startGame",
		"Java_com_dsemu_drastic_DraSticJNI_updateFrame",
		"Java_com_dsemu_drastic_DraSticJNI_renderFrame",
		"Java_com_dsemu_drastic_DraSticJNI_updateInput",
		"Java_com_dsemu_drastic_DraSticJNI_saveState",
		"Java_com_dsemu_drastic_DraSticJNI_loadState",
		"Java_com_dsemu_drastic_DraSticJNI_resetDS",
		"Java_com_dsemu_drastic_DraSticJNI_quitSystem",
		"Java_com_dsemu_drastic_DraSticJNI_getScreenBuffers",
		"Java_com_dsemu_drastic_DraSticJNI_getScreenshot",
		"Java_com_dsemu_drastic_DraSticJNI_getCpuType",
		NULL
	};
	int missing = 0;
	for (int i = 0; syms[i]; i++) {
		void *s = dlsym(h, syms[i]);
		if (!s) {
			printf("  MISSING %s\n", syms[i]);
			missing++;
		} else {
			printf("  ok      %s\n", syms[i]);
		}
	}

	printf(missing ? "SPIKE: FAILED (%d JNI symbols missing)\n"
	                : "SPIKE: SUCCESS (all JNI symbols resolve)\n",
	       missing);
	return missing ? 1 : 0;
}