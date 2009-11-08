
#include "PrecompiledHeader.h"
#include "IsoFS.h"
#include "IsoFile.h"

//////////////////////////////////////////////////////////////////////////
// IsoDirectory
//////////////////////////////////////////////////////////////////////////

// Used to load the Root directory from an image
IsoDirectory::IsoDirectory(SectorSource& r) :
	internalReader(r)
{
	u8 sector[2048];

	internalReader.readSector(sector,16);

	IsoFileDescriptor rootDirEntry(sector+156,38);

	Init(rootDirEntry);
}

// Used to load a specific directory from a file descriptor
IsoDirectory::IsoDirectory(SectorSource& r, IsoFileDescriptor directoryEntry) :
	internalReader(r)
{
	Init(directoryEntry);
}

IsoDirectory::~IsoDirectory() throw()
{
}

void IsoDirectory::Init(const IsoFileDescriptor& directoryEntry)
{
	// parse directory sector
	IsoFile dataStream (internalReader, directoryEntry);

	files.clear();

	int remainingSize = directoryEntry.size;

	u8 b[257];

	while(remainingSize>=4) // hm hack :P
	{
		b[0] = dataStream.read<u8>();

		if(b[0]==0)
		{
			break; // or continue?
		}

		remainingSize -= b[0];

		dataStream.read(b+1, b[0]-1);

		files.push_back(IsoFileDescriptor(b, b[0]));
	}

	b[0] = 0;
}

const IsoFileDescriptor& IsoDirectory::GetEntry(int index) const
{
	return files[index];
}

int IsoDirectory::GetIndexOf(const wxString& fileName) const
{
	for(unsigned int i=0;i<files.size();i++)
	{
		if(files[i].name == fileName) return i;
	}

	throw Exception::FileNotFound( fileName );
}

const IsoFileDescriptor& IsoDirectory::GetEntry(const wxString& fileName) const
{
	return GetEntry(GetIndexOf(fileName));
}

IsoFileDescriptor IsoDirectory::FindFile(const wxString& filePath) const
{
	pxAssert( !filePath.IsEmpty() );

	// wxWidgets DOS-style parser should work fine for ISO 9660 path names.  Only practical difference
	// is case sensitivity, and that won't matter for path splitting.
	wxFileName parts( filePath, wxPATH_DOS );
	IsoFileDescriptor info;
	const IsoDirectory* dir = this;
	ScopedPtr<IsoDirectory> deleteme;

	// walk through path ("." and ".." entries are in the directories themselves, so even if the
	// path included . and/or .., it still works)

	for(uint i=0; i<parts.GetDirCount(); ++i)
	{
		info = dir->GetEntry(parts.GetDirs()[i]);
		if(info.IsFile()) throw Exception::FileNotFound( filePath );
		
		dir = deleteme = new IsoDirectory(internalReader, info);
	}

	if( !parts.GetFullName().IsEmpty() )
		info = dir->GetEntry(parts.GetFullName());

	return info;
}

bool IsoDirectory::IsFile(const wxString& filePath) const
{
	if( filePath.IsEmpty() ) return false;
	return (FindFile(filePath).flags&2) != 2;
}

bool IsoDirectory::IsDir(const wxString& filePath) const
{
	if( filePath.IsEmpty() ) return false;
	return (FindFile(filePath).flags&2) == 2;
}

u32 IsoDirectory::GetFileSize( const wxString& filePath ) const
{
	return FindFile( filePath ).size;
}

IsoFileDescriptor::IsoFileDescriptor()
{
	lba = 0;
	size = 0;
	flags = 0;
}

IsoFileDescriptor::IsoFileDescriptor(const u8* data, int length)
{
	lba		= (u32&)data[2];
	size	= (u32&)data[10];

	date.year      = data[18] + 1900;
	date.month     = data[19];
	date.day       = data[20];
	date.hour      = data[21];
	date.minute    = data[22];
	date.second    = data[23];
	date.gmtOffset = data[24];

	flags = data[25];

	// This assert probably means a coder error, but let's fall back on a runtime exception
	// in release builds since, most likely, the error is "recoverable" form a user standpoint.
	if( !pxAssertDev( (lba>=0) && (length>=0), "Invalid ISO file descriptor data encountered." ) )
		throw Exception::BadStream();

	int fileNameLength = data[32];

	if(fileNameLength==1)
	{
		u8 c = data[33];

		switch(c)
		{
			case 0:	name = L"."; break;
			case 1: name = L".."; break;
			default: name = (wxChar)c;
		}
	}
	else
	{
		// copy string and up-convert from ascii to wxChar

		const u8* fnsrc = data+33;
		const u8* fnend = fnsrc+fileNameLength;

		while( fnsrc != fnend )
		{
			name += (wxChar)*fnsrc;
			++fnsrc;
		}
	}
}
