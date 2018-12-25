/*
 * CopyDetails - A tool to copy some properties and dates from one video file to another
 * Copyright(C) 2018 Tamas Kezdi
 *
 * This program is free software : you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see < https://www.gnu.org/licenses/>.
 */

#include <windows.h>
#include <shellapi.h>
#include <shobjidl.h>

template<typename T, int NUM_ELEMETS>
constexpr int GetNumElements(T (&arg)[NUM_ELEMETS]) { return NUM_ELEMETS; }

constexpr int cMaxPath = 32767;
constexpr int cMaxNumProperties = 1024;

constexpr const wchar_t cUsageMessage[] =
	L"Usage:\n"
	L"\n"
	L"  CopyDetails.exe target_file source_file\n";

constexpr const wchar_t cCannotInitializeCOM[] = L"Cannot initialize COM library\n";
constexpr const wchar_t cCannotGetCommandLine[] = L"Cannot get command line\n";
constexpr const wchar_t cCannotGetFullPath[] = L"Cannot get full path for file: ";
constexpr const wchar_t cCannotOpenFile[] = L"Cannot open file: ";
constexpr const wchar_t cCannotGetFiletime[] = L"Cannot get filetime\n";
constexpr const wchar_t cCannotSetFiletime[] = L"Cannot set filetime\n";
constexpr const wchar_t cCannotGetPropertyStore[] = L"Cannot get property store for file: ";
constexpr const wchar_t cCannotGetNumberOfProperties[] = L"Cannot get number of properties\n";
constexpr const wchar_t cCannotGetPropertyKey[] = L"Cannot get property key: ";
constexpr const wchar_t cCannotReadProperty[] = L"Cannot read existing property : ";
constexpr const wchar_t cCannotReadUnknownProperty[] = L"Cannot read unknown property\n";
constexpr const wchar_t cCannotWriteProperty[] = L"Cannot write property : ";
constexpr const wchar_t cCannotWriteUnknownProperty[] = L"Cannot write unknown property\n";
constexpr const wchar_t cCannotCommitProperty[] = L"Cannot commit property : ";
constexpr const wchar_t cCannotCommitUnknownProperty[] = L"Cannot commit unknown property\n";
constexpr const wchar_t cCommitFailed[] = L"Commit failed\n";
constexpr const wchar_t cNoPropertyToCommit[] = L"No property tocommit\n";
constexpr const wchar_t cNewLine[] = L"\n";

struct PropertyFormat
{
	GUID mFormatId;
	int mPropertyStartIndex;
	int mPropertyEndIndex;
};

struct PropertyId
{
	int mFormatIdIndex;
	DWORD mPropertyId;
};

struct FileProperties
{
	const wchar_t *mFilePath;
	IPropertyStore *mPropertyStore;
	DWORD mNumProperties;

	void Init(GETPROPERTYSTOREFLAGS flags);
	void InitNumProperties();
	void Dispose();
	void Read();
	void Write();
};

HANDLE gConsoleOutput;
HANDLE gFile;

FILETIME gCreationTime;
FILETIME gLastWriteTime;

DWORD gWrittenOut;
PROPERTYKEY gCurrPropertyKey;
wchar_t gFullPathDest[cMaxPath];
wchar_t gFullPathSrc[cMaxPath];

int gArgC;
LPWSTR *gArgV;

constexpr PropertyId gPropertyIds[] =
{
	{0, 100}, // System.Media.DateEncoded
	{1,   5}, // System.Media.Year
	{1,  38}, // System.Media.SubTitle
	{2,  13}, // System.Media.ClassPrimaryID
	{2,  14}, // System.Media.ClassSecondaryID
	{2,  15}, // System.Media.DVDID
	{2,  16}, // System.Media.MCDI
	{2,  17}, // System.Media.MetadataContentProvider
	{2,  18}, // System.Media.ContentDistributor
	{2,  22}, // System.Media.Producer
	{2,  23}, // System.Media.Writer
	{2,  24}, // System.Media.CollectionGroupID
	{2,  25}, // System.Media.CollectionID
	{2,  26}, // System.Media.ContentID
	{2,  27}, // System.Media.CreatorApplication
	{2,  28}, // System.Media.CreatorApplicationVersion
	{2,  30}, // System.Media.Publisher
	{2,  32}, // System.Media.AuthorUrl
	{2,  33}, // System.Media.PromotionUrl
	{2,  34}, // System.Media.UserWebUrl
	{2,  35}, // System.Media.UniqueFileIdentifier
	{2,  36}, // System.Media.EncodedBy
	{2,  38}, // System.Media.ProtectionType
	{2,  39}, // System.Media.ProviderRating
	{2,  40}, // System.Media.ProviderStyle
	{2,  41}, // System.Media.UserNoAutoInfo
	{2,  42}, // System.Media.SeriesName
	{2,  47}, // System.Media.ThumbnailLargePath
	{2,  48}, // System.Media.ThumbnailLargeUri
	{2,  49}, // System.Media.ThumbnailSmallPath
	{2,  50}, // System.Media.ThumbnailSmallUri
	{2, 100}, // System.Media.EpisodeNumber
	{2, 101}, // System.Media.SeasonNumber
	{3, 100}, // System.Media.SubscriptionContentId
	{4, 100}, // System.Media.DlnaProfileID
	{5, 100}  // System.Media.DateReleased
};

constexpr int GetPropertyStartIndex(int format_index) noexcept
{
	for (int i = 0; i < GetNumElements(gPropertyIds); ++i)
	{
		if (format_index == gPropertyIds[i].mFormatIdIndex)
			return i;
	}
}

constexpr int GetPropertyEndIndex(int format_index) noexcept
{
	for (int i = GetNumElements(gPropertyIds) - 1; i >= 0; --i)
	{
		if (format_index == gPropertyIds[i].mFormatIdIndex)
			return i + 1;
	}
}

constexpr PropertyFormat gPropertyKeyFormats[] =
{
	{{0x2E4B640D, 0x5019, 0x46D8, {0x88, 0x81, 0x55, 0x41, 0x4C, 0xC5, 0xCA, 0xA0}}, GetPropertyStartIndex(0), GetPropertyEndIndex(0)},
	{{0x56A3372E, 0xCE9C, 0x11D2, {0x9F, 0x0E, 0x00, 0x60, 0x97, 0xC6, 0x86, 0xF6}}, GetPropertyStartIndex(1), GetPropertyEndIndex(1)},
	{{0x64440492, 0x4C8B, 0x11D1, {0x8B, 0x70, 0x08, 0x00, 0x36, 0xB1, 0x1A, 0x03}}, GetPropertyStartIndex(2), GetPropertyEndIndex(2)},
	{{0x9AEBAE7A, 0x9644, 0x487D, {0xA9, 0x2C, 0x65, 0x75, 0x85, 0xED, 0x75, 0x1A}}, GetPropertyStartIndex(3), GetPropertyEndIndex(3)},
	{{0xCFA31B45, 0x525D, 0x4998, {0xBB, 0x44, 0x3F, 0x7D, 0x81, 0x54, 0x2F, 0xA4}}, GetPropertyStartIndex(4), GetPropertyEndIndex(4)},
	{{0xDE41CC29, 0x6971, 0x4290, {0xB4, 0x72, 0xF5, 0x9F, 0x2E, 0x2F, 0x31, 0xE2}}, GetPropertyStartIndex(5), GetPropertyEndIndex(5)}
};

PROPVARIANT gPropertyValues[GetNumElements(gPropertyIds)];

FileProperties gSrcFilePropertes;
FileProperties gDestFilePropertes;

int CompareGuid(const GUID &guid1, const GUID &guid2) noexcept
{
	if (guid1.Data1 < guid2.Data1)
		return -1;
	if (guid1.Data1 > guid2.Data1)
		return 1;

	if (guid1.Data2 < guid2.Data2)
		return -1;
	if (guid1.Data2 > guid2.Data2)
		return 1;

	if (guid1.Data3 < guid2.Data3)
		return -1;
	if (guid1.Data3 > guid2.Data3)
		return 1;

	for (int i = 0; i < GetNumElements(guid1.Data4); ++i)
	{
		if (guid1.Data4[i] < guid2.Data4[i])
			return -1;
		if (guid1.Data4[i] > guid2.Data4[i])
			return 1;
	}

	return 0;
}

template<int LENGTH>
void PrintA(const wchar_t (&message)[LENGTH])
{
	WriteConsole(gConsoleOutput, message, LENGTH - 1, &gWrittenOut, nullptr);
}

void PrintP(const wchar_t *message)
{
	WriteConsole(gConsoleOutput, message, lstrlen(message), &gWrittenOut, nullptr);
}

void PrintN(unsigned int number)
{
	const unsigned int n = number / 10;
	if (n)
		PrintN(n);

	const wchar_t digit = number % 10 + '0';
	WriteConsole(gConsoleOutput, &digit, 1, &gWrittenOut, nullptr);
}

void FileProperties::Init(GETPROPERTYSTOREFLAGS flags)
{
	mNumProperties = 0;
	if (FAILED(SHGetPropertyStoreFromParsingName(mFilePath, nullptr, flags, IID_PPV_ARGS(&mPropertyStore))))
	{
		PrintA(cCannotGetPropertyStore);
		PrintP(mFilePath);
		PrintA(cNewLine);
		return;
	}
}

void FileProperties::InitNumProperties()
{
	if (FAILED(mPropertyStore->GetCount(&mNumProperties)))
		PrintA(cCannotGetNumberOfProperties);
}

void FileProperties::Dispose()
{
	if (mPropertyStore)
	{
		mPropertyStore->Release();
		mPropertyStore = nullptr;
		mNumProperties = 0;
	}
}

void FileProperties::Read()
{
	for (DWORD i = 0; i < mNumProperties; ++i)
	{
		if (S_OK != mPropertyStore->GetAt(i, &gCurrPropertyKey))
		{
			PrintA(cCannotGetPropertyKey);
			PrintN(i);
			PrintA(cNewLine);
			continue;
		}

		for (int j = 0; j < GetNumElements(gPropertyKeyFormats); ++j)
		{
			const int compres = CompareGuid(gCurrPropertyKey.fmtid, gPropertyKeyFormats[j].mFormatId);
			if (0 < compres)
				continue;
			if (0 > compres)
				break;

			for (int k = gPropertyKeyFormats[j].mPropertyStartIndex; k < gPropertyKeyFormats[j].mPropertyEndIndex; ++k)
			{
				if (gCurrPropertyKey.pid > gPropertyIds[k].mPropertyId)
					continue;
				if (gCurrPropertyKey.pid < gPropertyIds[k].mPropertyId)
					break;

				if (FAILED(mPropertyStore->GetValue(gCurrPropertyKey, &gPropertyValues[k])))
				{
					PWSTR property_name;
					if (SUCCEEDED(PSGetNameFromPropertyKey(gCurrPropertyKey, &property_name)))
					{
						PrintA(cCannotReadProperty);
						PrintP(property_name);
						PrintA(cNewLine);
						CoTaskMemFree(property_name);
					}
					else
						PrintA(cCannotReadUnknownProperty);
				}
				break;
			}
			break;
		}
	}
}

void FileProperties::Write()
{
	for (int i = 0; i < GetNumElements(gPropertyKeyFormats); ++i)
	{
		gCurrPropertyKey.fmtid = gPropertyKeyFormats[i].mFormatId;

		for (int j = gPropertyKeyFormats[i].mPropertyStartIndex; j < gPropertyKeyFormats[i].mPropertyEndIndex; ++j)
		{
			if (VT_EMPTY == gPropertyValues[j].vt)
				continue;

			gCurrPropertyKey.pid = gPropertyIds[j].mPropertyId;

			Init(GPS_READWRITE);
			if (FAILED(mPropertyStore->SetValue(gCurrPropertyKey, gPropertyValues[j])))
			{
				PWSTR property_name;
				if (SUCCEEDED(PSGetNameFromPropertyKey(gCurrPropertyKey, &property_name)))
				{
					PrintA(cCannotWriteProperty);
					PrintP(property_name);
					PrintA(cNewLine);
					CoTaskMemFree(property_name);
				}
				else
					PrintA(cCannotWriteUnknownProperty);
			}
			else if (FAILED(mPropertyStore->Commit()))
			{
				PWSTR property_name;
				if (SUCCEEDED(PSGetNameFromPropertyKey(gCurrPropertyKey, &property_name)))
				{
					PrintA(cCannotCommitProperty);
					PrintP(property_name);
					PrintA(cNewLine);
					CoTaskMemFree(property_name);
				}
				else
					PrintA(cCannotCommitUnknownProperty);
			}
			// After Commit IPropertyStore cannot be usd any more, so Dispose
			Dispose();
		}
	}
}

void ProgramEntry()
{
	gConsoleOutput = GetStdHandle(STD_OUTPUT_HANDLE);
	gArgV = CommandLineToArgvW(GetCommandLine(), &gArgC);

	if (nullptr == gArgV)
	{
		PrintA(cCannotGetCommandLine);
		ExitProcess(1);
	}

	if (3 != gArgC)
	{
		PrintA(cUsageMessage);
		ExitProcess(0);
	}

	if (0 == GetFullPathName(gArgV[1], GetNumElements(gFullPathDest), gFullPathDest, nullptr))
	{
		PrintA(cCannotGetFullPath);
		PrintP(gArgV[1]);
		PrintA(cNewLine);
		ExitProcess(1);
	}

	if (0 == GetFullPathName(gArgV[2], GetNumElements(gFullPathSrc), gFullPathSrc, nullptr))
	{
		PrintA(cCannotGetFullPath);
		PrintP(gArgV[2]);
		PrintA(cNewLine);
		ExitProcess(1);
	}

	// CoInitializeEx must be called otherwise SHGetPropertyStoreFromParsingName won't work
	if (SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)))
	{
		gSrcFilePropertes.mFilePath = gFullPathSrc;
		gSrcFilePropertes.Init(GPS_DEFAULT);
		gSrcFilePropertes.InitNumProperties();
		if (gSrcFilePropertes.mNumProperties)
		{
			gSrcFilePropertes.Read();
			gDestFilePropertes.mFilePath = gFullPathDest;
			gDestFilePropertes.Write();
		}
		gSrcFilePropertes.Dispose();
		CoUninitialize();
	}
	else
		PrintA(cCannotInitializeCOM);

	gFile = CreateFile(gFullPathSrc, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (INVALID_HANDLE_VALUE == gFile)
	{
		PrintA(cCannotOpenFile);
		PrintP(gArgV[2]);
		PrintA(cNewLine);
		ExitProcess(1);
	}

	if (!GetFileTime(gFile, &gCreationTime, nullptr, &gLastWriteTime))
	{
		PrintA(cCannotGetFiletime);
		CloseHandle(gFile);
		ExitProcess(1);
	}
	CloseHandle(gFile);

	gFile = CreateFile(gFullPathDest, FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (INVALID_HANDLE_VALUE == gFile)
	{
		PrintA(cCannotOpenFile);
		PrintP(gArgV[1]);
		PrintA(cNewLine);
		ExitProcess(1);
	}

	if (!SetFileTime(gFile, &gCreationTime, nullptr, &gLastWriteTime))
	{
		PrintA(cCannotSetFiletime);
		CloseHandle(gFile);
		ExitProcess(1);
	}
	CloseHandle(gFile);

	ExitProcess(0);
}
