#pragma once

const UINT32 cMaxStoredSeqs = 0xfffffff0;			// can store up to this many sequences
const UINT32 cMinSeqStoreLen = 0x01;				// any individual sequence must be >= this min length
const UINT32 cMaxSeqStoreLen = 0xfffffff0;			// any individual sequence must be <= this max length
const UINT32 cAllocNumSeqHdrs = 50000;				// allocate for this incremental number of SeqHdrs
const UINT32 cAllocDescrSeqMem = (cAllocNumSeqHdrs * 1000);	// allocate for this incremental descr + seq bytes


typedef UINT32 tSeqID;		// sequence identifiers are 32bit 

#pragma pack(1)
typedef struct TAG_sSeqHdr {
	tSeqID SeqID;			// uniquely identifies sequence
	UINT32 Flags;			// any flags associated with this sequence
	UINT8 DescrLen;			// sequence descriptor is this length
	UINT32 SeqLen;			// sequence is this length
	UINT64 DescrSeqOfs;		// offset into descriptor followed by sequence	
	} tsSeqHdr;
#pragma pack()

class CSeqStore
{
	UINT32 m_NumStoredSeqs;			// this many sequences currently stored
	size_t m_TotStoredSeqsLen;		// total length of all currently stored sequences
	size_t m_UsedSeqHdrMem;			// tsSeqHdrs occupying this much memory 
	size_t m_AllocdSeqHdrSize;		// currently this much memory allocated to hold tsSeqHdrs
	tsSeqHdr *m_pSeqHdrs;			// allocated to hold tsSeqHdrs
	size_t m_UsedDescrSeqsMem;		// concatenated descriptors plus sequences currently require this much memory 
	size_t m_AllocdDescrSeqsSize;	// currently this much memory has been allocated to hold concatenated descriptors plus sequences
	UINT8 *m_pDescrSeqs;			// allocated to hold concatenated descriptors plus sequences
	
public:
	CSeqStore();
	~CSeqStore();
	int Reset(void);

	tSeqID			// identifier by which this sequence can later be retreived (0 if unable to add sequence)
		AddSeq(UINT32 Flags,			// any flags associated with this sequence
			   char *pszDescr,			// sequence descriptor
			   UINT32 SeqLen,			// sequence is this length
			   etSeqBase *pSeq);		// sequence to add


	tSeqID								// sequence identifier of sequence matching on same descriptor
		GetSeqID(char *pszDescr);		// must match on this descriptor

	UINT32								// returned number of currently stored sequences
			GetNumSeqs(void);			// get number of currently stored sequences

	size_t								// returned total length of all currently stored sequences
			GetTotalLen(void);			// get total length of all currently stored sequences

	UINT32								// returned flags
			GetFlags(tSeqID SeqID);		// get flags associated with sequence identified by SeqID

	UINT32								// returned sequence length
			GetLen(tSeqID SeqID);		// get sequence length identified by SeqID

	UINT32								// returned flags
			GetDescr(tSeqID SeqID,		// get descriptor associated with sequence identified by SeqID
					int MaxDescrLen,	// return at most this many descriptor chars copied into
					char *pszDescr);	// this returned sequence descriptor

	UINT32								// previous flags
			SetFlags(tSeqID SeqID,		// set flags associated with sequence identified by SeqID
					UINT32 Flags);		// flags to be associated with this sequence

	UINT32								// returned sequence copied into pRetSeq is this length; 0 if errors
			GetSeq(tSeqID SeqID,		// get sequence identified by SeqID
				UINT32 StartOfs,		// starting from this sequence offset
				UINT32 MaxSeqSize,		// return at most this many sequence bases  
				etSeqBase *pRetSeq);	// copy sequence into this caller allocated buffer 
};

