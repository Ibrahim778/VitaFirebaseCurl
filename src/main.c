#include <vitasdk.h>
#include <common/debugScreen.h>
#include <stdlib.h>
#include <stdio.h>
#include <curl/curl.h>
#include "ime.c"

#define println psvDebugScreenPrintfln
#define printf psvDebugScreenPrintf

#define PRESS_CONTINUE(b_c, str) do { psvDebugScreenPrintfln("Press %s to continue", str); SceCtrlData pad; do { sceCtrlPeekBufferPositive(0, &pad, 1); } while(!(pad.buttons & b_c)); } while(0)
#define TRY(method) do { psvDebugScreenPrintfln("Trying " #method); void *ret = (void *)method; psvDebugScreenPrintfln("Got ret = 0x%X", ret); } while(0);
#define TRY_RET(method, toSet, type) do { psvDebugScreenPrintfln("Trying " #method); void *ret = (void *)method; psvDebugScreenPrintfln("Got ret = 0x%X", ret); toSet = (type)ret;  } while(0)
#define DO(method) psvDebugScreenPrintfln("Doing "#method);

#define FAIL_IF(con) if (con) { psvDebugScreenPrintfln("Error occoured!"); sceKernelDelayThread(10 * 1000000); sceKernelExitProcess(0); } else psvDebugScreenPrintfln("Check PASSED");

#define netParamSize 4 * 1024 * 1024

#define toInt(a) (int)strtol(a, (char **)NULL, 10)

//Upload Params
#define UploadLink "https://vita-a73b4-default-rtdb.firebaseio.com/"
#define baseData "{\"name\":\"%s\", \"score\":%d}"

PsvDebugScreenFont defaultFont;

void printBuff(uint16_t *buff, int length)
{
    for (int i = 0; i < length; i++)
    {
        println("char at %i is %c", i, buff[i]);   
    }
}

void netInit() 
{
	psvDebugScreenPrintf("Loading module SCE_SYSMODULE_NET\n");
	TRY(sceSysmoduleLoadModule(SCE_SYSMODULE_NET));
	
	psvDebugScreenPrintf("Running sceNetInit\n");
	SceNetInitParam netInitParam;
    int size = netParamSize;
	netInitParam.memory = malloc(size);
	netInitParam.size = size;
	netInitParam.flags = 0;
	TRY(sceNetInit(&netInitParam));

	psvDebugScreenPrintf("Running sceNetCtlInit\n");
	TRY(sceNetCtlInit());
}

void netTerm() 
{
	psvDebugScreenPrintf("Running sceNetCtlTerm\n");
	sceNetCtlTerm();

	psvDebugScreenPrintf("Running sceNetTerm\n");
	sceNetTerm();

	psvDebugScreenPrintf("Unloading module SCE_SYSMODULE_NET\n");
	TRY(sceSysmoduleUnloadModule(SCE_SYSMODULE_NET));
}

void httpsInit()
{
    TRY(sceSysmoduleLoadModule(SCE_SYSMODULE_HTTPS));
    TRY(sceHttpInit(netParamSize));
}

void httpInit() 
{
	psvDebugScreenPrintf("Loading module SCE_SYSMODULE_HTTP\n");
	TRY(sceSysmoduleLoadModule(SCE_SYSMODULE_HTTP));

	psvDebugScreenPrintf("Running sceHttpInit\n");
	TRY(sceHttpInit(netParamSize));
}

void httpTerm() 
{
	psvDebugScreenPrintf("Running sceHttpTerm\n");
	TRY(sceHttpTerm());

	psvDebugScreenPrintf("Unloading module SCE_SYSMODULE_HTTP\n");
	TRY(sceSysmoduleUnloadModule(SCE_SYSMODULE_HTTP));
}

void httpsTerm()
{
    println("Unloading SCE_SYSMODULE_HTTP");
    TRY(sceSysmoduleUnloadModule(SCE_SYSMODULE_HTTPS));
}

int uploadFile(CURL *curl, char *name, int score, char *file)
{
    int res;
    //Json data
    char data[0x100] = {0};

    //FileName
    char fileName[0x100] = {0};
    sprintf(fileName, "%s%s.json", UploadLink, file);

    //Definetly NOT the best way of doing it but hey, if it works it works
    sprintf(data, baseData, name, score);
    println("Got data \"%s\"", data);
    PRESS_CONTINUE(SCE_CTRL_START, "START");

	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/58.0.3029.110 Safari/537.36");
	curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
    curl_easy_setopt(curl, CURLOPT_URL, fileName);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(data));
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
    curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);

    TRY_RET(curl_easy_perform(curl), res, int);
    return res;
}

//Scale font 2^power
PsvDebugScreenFont *scaleFont(PsvDebugScreenFont * font, int power)
{
    for (int i = 0; i < power; i++)
    {
        font = psvDebugScreenScaleFont2x(font);
    }
    return font;
}

int getImeInput(uint16_t *output, unsigned short *title, unsigned short *initialText)
{
    uint16_t input[SCE_IME_DIALOG_MAX_TEXT_LENGTH + 1] = {0};
    SceImeDialogParam param;

    int shown_diag = 0;
    int said_something = 0;
    sceAppUtilInit(&(SceAppUtilInitParam){}, &(SceAppUtilBootParam){});
    sceCommonDialogSetConfigParam(&(SceCommonDialogConfigParam){});
    
    gxm_init();

    while(!said_something)
    {
        memset(dbuf[backBufferIndex].data, 0xff000000, DISPLAY_HEIGHT*DISPLAY_STRIDE_IN_PIXELS * 4);
        
        if(!shown_diag)
        {
            sceImeDialogParamInit(&param);
            param.supportedLanguages = SCE_IME_LANGUAGE_ENGLISH_GB | SCE_IME_LANGUAGE_ENGLISH;
			param.languagesForced = SCE_TRUE;
			param.type = SCE_IME_DIALOG_TEXTBOX_MODE_DEFAULT;
			param.option = 0;
			param.textBoxMode = SCE_IME_DIALOG_TEXTBOX_MODE_DEFAULT;
			param.title = title;
			param.maxTextLength = SCE_IME_DIALOG_MAX_TEXT_LENGTH;
			param.initialText = initialText;
			param.inputTextBuffer = input;
            shown_diag = sceImeDialogInit(&param) > 0;
        }

        if(sceImeDialogGetStatus() == SCE_COMMON_DIALOG_STATUS_FINISHED)
        {
            SceImeDialogResult result = (SceImeDialogResult){};
            sceImeDialogGetResult(&result);
            uint16_t *last_input = (result.button == SCE_IME_DIALOG_BUTTON_ENTER) ? input : u"";
            said_something = memcmp(last_input, u"", strlen((char *)last_input) * sizeof(' '));
            sceImeDialogTerm();
            shown_diag = 0;
        }

        sceCommonDialogUpdate(&(SceCommonDialogUpdateParam)
        {{
            NULL, dbuf[backBufferIndex].data, 0, 0,
            DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_STRIDE_IN_PIXELS },
            dbuf[backBufferIndex].sync
        });

        gxm_swap();
        sceDisplayWaitVblankStart();
    }

    gxm_term();
    psvDebugScreenInit();
    memcpy(output, input, 513);
    return 0;
}

int convertToString(uint16_t *ime_result, char *output, int length)
{
    for (int i = 0; i < length; i ++)
    {
        output[i] = ime_result[i];
    }
}

//Just for cleanup between ime and other code
int uploadFirebase(char *name, int score, char *file)
{
    netInit();
    PRESS_CONTINUE(SCE_CTRL_CROSS, "X");

    httpInit();
    PRESS_CONTINUE(SCE_CTRL_CIRCLE, "O");

    httpsInit();
    PRESS_CONTINUE(SCE_CTRL_TRIANGLE, "Triangle");

    println("Doing curl init");
    CURL *curl = curl_easy_init();
    FAIL_IF(!curl)

    println("Init done!");

    int res;
    TRY_RET(uploadFile(curl, name, score, file), res, int);
    FAIL_IF(res != CURLE_OK);

    psvDebugScreenSetFont(scaleFont(psvDebugScreenGetFont(), 3));

    psvDebugScreenClear(0);
    println("MISSION SUCCESS");
    psvDebugScreenSetFont(&defaultFont);
    PRESS_CONTINUE(SCE_CTRL_CROSS, "X");


    DO(curl_easy_cleanup(curl));

    println("Success!");
    
    httpTerm();
    httpsTerm();
    netTerm();

}

int main()
{
    backBufferIndex = 0;
    frontBufferIndex = 0;
    psvDebugScreenInit();
    memcpy(&defaultFont, psvDebugScreenGetFont(), sizeof(defaultFont));
    //psvDebugScreenSetFont(scaleFont(psvDebugScreenGetFont(), 1));

    println("Hello World!");

    uint16_t result[SCE_IME_DIALOG_MAX_TEXT_LENGTH + 1] = {0};
    getImeInput(result, u"Enter Name", u"Player");
    
    char nameStr[513/2];
    convertToString(result, nameStr, 513);

    uint16_t scoreResult[513];

    getImeInput(scoreResult, u"Enter score (numbers only!)", u"10");
    char scorebuff[256];
    convertToString(scoreResult, scorebuff, 513);
    int score = toInt(scorebuff);

    uint16_t fileName[513];
    getImeInput(fileName, u"Enter File Name (.json will be added automatically)", u"vita");

    char fileString[256];
    convertToString(fileName, fileString, 513);

    println("Got Score %d", score);
    println("Got name %s", nameStr);
    println("Got file name %s", fileString);

    uploadFirebase(nameStr, score, fileString);
    

    PRESS_CONTINUE(SCE_CTRL_TRIANGLE, "Triangle");
}