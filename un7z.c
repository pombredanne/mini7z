/// zun7z - lzmasdk.c example to extract archives supported by 7z.so ///
/// multibyte filenames are not tested ///
/// cannot handle multivolume archives ///

#include "../lib/lzma.h"
#include <string.h>

bool wildmatch(const char *pattern, const char *compare){
	switch(*pattern){
		case '?': //0x3f
			return *compare&&wildmatch(pattern+1,compare+1);
		case '*': //0x2a
			return wildmatch(pattern+1,compare)||(*compare&&wildmatch(pattern,compare+1));
		default:
			if(!*pattern&&!*compare)return true;
			if(*pattern!=*compare)return false;
			return wildmatch(pattern+1,compare+1);
	}
}

bool matchwildcard2(const char *wildcard, const char *string, const int iMode){
	int u=0,u1=0,u2=0;
	char pattern[768];
	char compare[768];
	if(!wildcard||!string)return 0;
	strcpy(pattern,wildcard);
	strcpy(compare,string);

	for(u=0;u<strlen(pattern);u++){
		pattern[u]=upcase(pattern[u]);
		if(pattern[u]=='\\')pattern[u]='/';
		if(pattern[u]=='/')u1++;
	}

	for(u=0;u<strlen(compare);u++){
		compare[u]=upcase(compare[u]);
		if(compare[u]=='\\')compare[u]='/';
		if(compare[u]=='/')u2++;
	}

	if(
		(iMode==wildmode_string)||
		(iMode==wildmode_samedir&&u1==u2)||
		(iMode==wildmode_recursive&&u1<=u2)
	)return wildmatch(pattern, compare);
	return false;
}

bool matchwildcard(const char *wildcard, const char *string){
	return matchwildcard2(wildcard, string, wildmode_string);
}

#include <stdio.h>
//#include <stdlib.h>
#include <string.h>

static WORD LastByte(LPCSTR lpszStr){
	int n=strlen(lpszStr);
	return n ? lpszStr[n-1] : 0;
}

static int match(const char *name, int argc, const char **argv){
  for(int i=0;i<argc;i++){
	if(matchwildcard(argv[i],name))return 0;
  }
  return 1;
}

typedef struct{
	IArchiveExtractCallback_vt *vt;
	u32 refs;
	SCryptoGetTextPasswordFixed setpassword;
	IInArchive_ *archiver;
	u32 lastIndex;
	int argc;
	const char **argv;
} SArchiveExtractCallbackFileList;

static HRESULT WINAPI SArchiveExtractCallbackFileList_GetStream(void* _self, u32 index, /*ISequentialOutStream_*/IOutStream_ **outStream, s32 askExtractMode){
	SArchiveExtractCallbackFileList *self = (SArchiveExtractCallbackFileList*)_self;
	*outStream = NULL;
	PROPVARIANT path;
	memset(&path,0,sizeof(PROPVARIANT));
	self->lastIndex = index;
	lzmaGetArchiveFileProperty(self->archiver, index, kpidPath, &path);
	int len = wcslen(path.bstrVal);
	char *fname = (char*)malloc(len*3);
	wcstombs(fname,path.bstrVal,len*3);
	PropVariantClear(&path);
	if(!match(fname,self->argc,self->argv)){
		printf("Extracting %s...\n",fname);
		makedir(fname);
		IOutStream_* stream = (IOutStream_*)calloc(1,sizeof(SOutStreamFile));
		MakeSOutStreamFile((SOutStreamFile*)stream,fname);
		*outStream = stream; // WILL BE RELEASED AUTOMATICALLY.
	}
	return S_OK;
}

static int extract(const char *password,const char *arc, const char *dir, int argc, const char **argv){
	if(lzmaOpen7z()){
		return -1;
	}
	lzmaLoadUnrar();
	SInStreamFile sin;
	MakeSInStreamFile(&sin,arc);
	void *archiver=NULL;
	unsigned char arctype=0;
	for(;arctype<0xff;arctype++){
		if(!lzmaCreateArchiver(&archiver,arctype,0,0)){
			sin.vt->Seek(&sin,0,SEEK_SET,NULL); // archiver should do this automatically lol lol lol
			if(!lzmaOpenArchive(archiver,&sin,password))break;
			lzmaDestroyArchiver(&archiver,0);
		}
	}
	if(!archiver){
		fprintf(stderr,"7z.so could not open the file as any archive.\n");
		sin.vt->Release(&sin);
		lzmaClose7z();
		return 1;
	}
	printf("arctype = %d .\n\n",arctype);

	SArchiveExtractCallbackFileList sextract;
	MakeSArchiveExtractCallbackBare((SArchiveExtractCallbackBare*)&sextract,(IInArchive_*)archiver,password);
	sextract.argc = argc;
	sextract.argv = argv;
	sextract.vt->GetStream = SArchiveExtractCallbackFileList_GetStream;
	lzmaExtractArchive(archiver, NULL, -1, 0, (IArchiveExtractCallback_*)&sextract);

	lzmaDestroyArchiver(&archiver,0);
	sin.vt->Release(&sin);
	lzmaUnloadUnrar();
	lzmaClose7z();

	return 0;
}

static int list(const char *password,const char *arc, int argc, const char **argv){
	if(lzmaOpen7z()){
		return -1;
	}
	SInStreamFile sin;
	MakeSInStreamFile(&sin,arc);
	void *archiver=NULL;
	unsigned char arctype=0;
	for(;arctype<0xff;arctype++){
		if(!lzmaCreateArchiver(&archiver,arctype,0,0)){
			sin.vt->Seek(&sin,0,SEEK_SET,NULL); // archiver should do this automatically lol lol lol
			if(!lzmaOpenArchive(archiver,&sin,password))break;
			lzmaDestroyArchiver(&archiver,0);
		}
	}
	if(!archiver){
		fprintf(stderr,"7z.so could not open the file as any archive.\n");
		sin.vt->Release(&sin);
		lzmaClose7z();
		return 1;
	}
	printf("arctype = %d .\n\n",arctype);
	int num_items;
	lzmaGetArchiveFileNum(archiver,(u32*)&num_items); // to deal with empty archive...
	printf("Name                                Method               PackedSize Size      \n");
	printf("----------------------------------- -------------------- ---------- ----------\n");
	for(int i=0;i<num_items;i++){
		PROPVARIANT propPath,propMethod,propPackedSize,propSize,propMTime;
		memset(&propPath,0,sizeof(PROPVARIANT));
		memset(&propMethod,0,sizeof(PROPVARIANT));
		memset(&propPackedSize,0,sizeof(PROPVARIANT));
		memset(&propSize,0,sizeof(PROPVARIANT));
		memset(&propMTime,0,sizeof(PROPVARIANT));
		lzmaGetArchiveFileProperty(archiver,i,kpidPath,&propPath);
		lzmaGetArchiveFileProperty(archiver,i,kpidMethod,&propMethod);
		lzmaGetArchiveFileProperty(archiver,i,kpidPackSize,&propPackedSize);
		lzmaGetArchiveFileProperty(archiver,i,kpidSize,&propSize);
		lzmaGetArchiveFileProperty(archiver,i,kpidSize,&propMTime); // displaying this to be implemented.
		wcstombs(cbuf,propPath.bstrVal,BUFLEN);
		if(!match(cbuf,argc,argv)){
			printf("%-35ls %-20ls %10llu %10llu\n",propPath.bstrVal,propMethod.bstrVal,propPackedSize.uhVal,propSize.uhVal);
		}
		PropVariantClear(&propPath);
		PropVariantClear(&propMethod);
	}
	printf("----------------------------------- -------------------- ---------- ----------\n");
	lzmaDestroyArchiver(&archiver,0);
	sin.vt->Release(&sin);
	lzmaClose7z();

	return 0;
}

int zun7z(int argc, const char **argv){
  printf("7z Extractor\nUsage: zun7z [xl][PASSWORD] arc.7z [extract_dir] [filespec]\n\n");
  if(argc<3)return -1;
  const char *w="*";
  
  switch(argv[1][0]){
	case 'x':{
		const char *password=argv[1]+1;
		const char *arc=argv[2];
		const char *dir=".";
		argv+=3;argc-=3;
		if(argv[0]&&(LastByte(argv[0])=='/'||LastByte(argv[0])=='\\')){makedir(argv[0]);dir=argv[0];argv++;argc--;}
		return extract(password,arc,dir,argc?argc:1,argc?argv:&w);
	}
	case 'l':return list(argv[1]+1,argv[2],argc-3?argc-3:1,argc-3?argv+3:&w);
	default:return -1;
  }
}

