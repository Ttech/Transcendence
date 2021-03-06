//	CTradingDesc.cpp
//
//	CTradingDesc class

#include "PreComp.h"

#define SELL_TAG								CONSTLIT("Sell")
#define BUY_TAG									CONSTLIT("Buy")

#define ACTUAL_PRICE_ATTRIB						CONSTLIT("actualPrice")
#define CURRENCY_ATTRIB							CONSTLIT("currency")
#define CREDIT_CONVERSION_ATTRIB				CONSTLIT("creditConversion")
#define CRITERIA_ATTRIB							CONSTLIT("criteria")
#define INVENTORY_ADJ_ATTRIB					CONSTLIT("inventoryAdj")
#define ITEM_ATTRIB								CONSTLIT("item")
#define MAX_ATTRIB								CONSTLIT("max")
#define PRICE_ADJ_ATTRIB						CONSTLIT("priceAdj")
#define REPLENISH_ATTRIB						CONSTLIT("replenish")

#define CONSTANT_PREFIX							CONSTLIT("constant")
#define UNAVAILABLE_PREFIX						CONSTLIT("unavailable")

CTradingDesc::CTradingDesc (void) : 
		m_iMaxCurrency(0),
		m_iReplenishCurrency(0)

//	CTradingDesc constructor

	{
	}

CTradingDesc::~CTradingDesc (void)

//	CTradingDesc destructor

	{
	}

void CTradingDesc::AddOrder (CItemType *pItemType, const CString &sCriteria, int iPriceAdj, DWORD dwFlags)

//	AddOrder
//
//	Add a new commodity line

	{
	int i;

	//	We always add at the beginning (because new orders take precedence)

	SCommodityDesc *pCommodity = m_List.InsertAt(0);
	pCommodity->pItemType = pItemType;
	if (pItemType == NULL)
		CItem::ParseCriteria(sCriteria, &pCommodity->ItemCriteria);
	pCommodity->PriceAdj.SetInteger(iPriceAdj);
	pCommodity->dwFlags = dwFlags;
	pCommodity->sID = ComputeID(pCommodity->pItemType.GetUNID(), sCriteria, dwFlags);

	//	If we find a previous order for the same criteria, then delete it

	for (i = 1; i < m_List.GetCount(); i++)
		if (strEquals(pCommodity->sID, m_List[i].sID))
			{
			m_List.Delete(i);
			break;
			}
	}

CString CTradingDesc::ComputeID (DWORD dwUNID, const CString &sCriteria, DWORD dwFlags)

//	ComputeID
//
//	Generates a string ID for the order. Two identical orders should have the same ID

	{
	CString sFlags;
	if (dwFlags & FLAG_SELLS)
		sFlags = CONSTLIT("S");
	else
		sFlags = CONSTLIT("B");

	if (dwUNID)
		return strPatternSubst(CONSTLIT("%s:%x"), sFlags, dwUNID);
	else
		return strPatternSubst(CONSTLIT("%s:%s"), sFlags, sCriteria);
	}

int CTradingDesc::ComputeMaxCurrency (CSpaceObject *pObj)

//	ComputeMaxCurrency
//
//	Computes max balance

	{
	return m_iMaxCurrency * (90 + ((pObj->GetDestiny() + 9) / 18)) / 100;
	}

int CTradingDesc::ComputePrice (CSpaceObject *pObj, const CItem &Item, const SCommodityDesc &Commodity)

//	ComputePrice
//
//	Computes the price of the item in the proper currency

	{
	bool bActual = (Commodity.dwFlags & FLAG_ACTUAL_PRICE ? true : false);

	//	Get the price 

	CurrencyValue iPrice;
	CString sPrefix;
	int iPriceAdj = Commodity.PriceAdj.EvalAsInteger(pObj, &sPrefix);

	if (sPrefix.IsBlank())
		iPrice = iPriceAdj * Item.GetTradePrice(pObj, bActual) / 100;
	else if (strEquals(sPrefix, CONSTANT_PREFIX))
		iPrice = iPriceAdj;
	else if (strEquals(sPrefix, UNAVAILABLE_PREFIX))
		return -1;
	else
		{
		kernelDebugLogMessage("Unknown priceAdj prefix: %s", sPrefix.GetASCIIZPointer());
		return -1;
		}

	//	Convert to proper currency

	return (int)m_pCurrency->Exchange(Item.GetCurrencyType(), iPrice);
	}

ALERROR CTradingDesc::CreateFromXML (SDesignLoadCtx &Ctx, CXMLElement *pDesc, CTradingDesc **retpTrade)

//	InitFromXML
//
//	Initialize from an XML element

	{
	ALERROR error;
	int i;

	//	Allocate the trade structure

	CTradingDesc *pTrade = new CTradingDesc;
	pTrade->m_pCurrency.LoadUNID(pDesc->GetAttribute(CURRENCY_ATTRIB));
	pTrade->m_iMaxCurrency = pDesc->GetAttributeIntegerBounded(MAX_ATTRIB, 0, -1, 0);
	pTrade->m_iReplenishCurrency = pDesc->GetAttributeIntegerBounded(REPLENISH_ATTRIB, 0, -1, 0);

	//	Allocate the array

	int iCount = pDesc->GetContentElementCount();
	if (iCount)
		{
		pTrade->m_List.InsertEmpty(iCount);

		//	Load

		for (i = 0; i < iCount; i++)
			{
			CXMLElement *pLine = pDesc->GetContentElement(i);
			SCommodityDesc *pCommodity = &pTrade->m_List[i];

			//	Parse criteria

			CString sCriteria = pLine->GetAttribute(CRITERIA_ATTRIB);
			if (!sCriteria.IsBlank())
				CItem::ParseCriteria(sCriteria, &pCommodity->ItemCriteria);
			else
				CItem::InitCriteriaAll(&pCommodity->ItemCriteria);

			//	Item

			pCommodity->pItemType.LoadUNID(Ctx, pLine->GetAttribute(ITEM_ATTRIB));

			//	Other

			if (error = pCommodity->PriceAdj.InitFromString(Ctx, pLine->GetAttribute(PRICE_ADJ_ATTRIB)))
				return error;

			if (error = pCommodity->InventoryAdj.InitFromString(Ctx, pLine->GetAttribute(INVENTORY_ADJ_ATTRIB)))
				return error;

			//	Flags

			pCommodity->dwFlags = 0;
			if (strEquals(pLine->GetTag(), BUY_TAG))
				pCommodity->dwFlags |= FLAG_BUYS;
			else if (strEquals(pLine->GetTag(), SELL_TAG))
				pCommodity->dwFlags |= FLAG_SELLS;

			if (pLine->GetAttributeBool(ACTUAL_PRICE_ATTRIB))
				pCommodity->dwFlags |= FLAG_ACTUAL_PRICE;

			if (!pCommodity->InventoryAdj.IsEmpty())
				pCommodity->dwFlags |= FLAG_INVENTORY_ADJ;

			//	Set ID

			pCommodity->sID = pTrade->ComputeID(pCommodity->pItemType.GetUNID(), sCriteria, pCommodity->dwFlags);
			}
		}

	//	Done

	*retpTrade = pTrade;

	return NOERROR;
	}

bool CTradingDesc::Buys (CSpaceObject *pObj, const CItem &Item, int *retiPrice, int *retiMaxCount)

//	Buys
//
//	Returns TRUE if the given object buys items of the given type.
//	Optionally returns a price and a max number.
//
//	Note that we always return a price for items we are willing to buy, even if we
//	don't currently have enough to buy it.

	{
	int i;

	//	Loop over the commodity list and find the first entry that matches

	for (i = 0; i < m_List.GetCount(); i++)
		if (Matches(Item, m_List[i])
				&& (m_List[i].dwFlags & FLAG_BUYS))
			{
			int iPrice = ComputePrice(pObj, Item, m_List[i]);
			if (iPrice < 0)
				return false;

			//	Compute the maximum number of this item that we are willing
			//	to buy. First we figure out how much money the station has left

			int iBalance = (int)pObj->GetBalance(m_pCurrency->GetUNID());
			int iMaxCount = (iPrice > 0 ? (iBalance / iPrice) : 0);

			//	Done

			if (retiMaxCount)
				*retiMaxCount = iMaxCount;

			if (retiPrice)
				*retiPrice = iPrice;

			return true;
			}

	return false;
	}

int CTradingDesc::Charge (CSpaceObject *pObj, int iCharge)

//	Charge
//
//	Charge out of the station's balance

	{
	if (m_iMaxCurrency)
		return (int)pObj->ChargeMoney(m_pCurrency->GetUNID(), iCharge);
	else
		return 0;
	}

bool CTradingDesc::Matches (const CItem &Item, const SCommodityDesc &Commodity)

//	Matches
//
//	Returns TRUE if the given item matches the commodity

	{
	if (Commodity.pItemType)
		return (Commodity.pItemType == Item.GetType());
	else
		return Item.MatchesCriteria(Commodity.ItemCriteria);
	}

void CTradingDesc::OnCreate (CSpaceObject *pObj)

//	OnCreate
//
//	Station is created

	{
	//	Give the station a limited amount of money

	if (m_iMaxCurrency)
		{
		int iMaxCurrency = ComputeMaxCurrency(pObj);
		pObj->CreditMoney(m_pCurrency->GetUNID(), iMaxCurrency);
		}
	}

ALERROR CTradingDesc::OnDesignLoadComplete (SDesignLoadCtx &Ctx)

//	OnDesignLoadComplete
//
//	Design loaded

	{
	ALERROR error;
	int i;

	if (error = m_pCurrency.Bind(Ctx))
		return error;

	for (i = 0; i < m_List.GetCount(); i++)
		if (error = m_List[i].pItemType.Bind(Ctx))
			return error;

	return NOERROR;
	}

void CTradingDesc::OnUpdate (CSpaceObject *pObj)

//	OnUpdate
//
//	Station updates (call roughly every 1800 ticks)

	{
	if (m_iMaxCurrency && m_iReplenishCurrency)
		{
		int iBalance = (int)pObj->GetBalance(m_pCurrency->GetUNID());
		int iMaxCurrency = ComputeMaxCurrency(pObj);

		if (iBalance < iMaxCurrency)
			pObj->CreditMoney(m_pCurrency->GetUNID(), m_iReplenishCurrency);
		}
	}

void CTradingDesc::ReadFromStream (SLoadCtx &Ctx)

//	ReadFromStream
//
//	Read from a stream
//
//	DWORD			m_pCurrency UNID
//	DWORD			m_iMaxCurrency
//	DWORD			m_iReplenishCurrency
//	DWORD			No of orders
//
//	CString			sID
//	DWORD			pItemType
//	CString			ItemCriteria
//	CFormulaText	PriceAdj
//	CFormulaText	InventoryAdj
//	DWORD			dwFlags

	{
	int i;
	DWORD dwLoad;

	if (Ctx.dwVersion >= 62)
		{
		Ctx.pStream->Read((char *)&dwLoad, sizeof(DWORD));
		m_pCurrency.Set(dwLoad);
		if (m_pCurrency == NULL)
			m_pCurrency.Set(DEFAULT_ECONOMY_UNID);
		}
	else
		{
		CString sDummy;
		sDummy.ReadFromStream(Ctx.pStream);
		Ctx.pStream->Read((char *)&dwLoad, sizeof(DWORD));

		//	Previous versions are always credits

		m_pCurrency.Set(DEFAULT_ECONOMY_UNID);
		}

	Ctx.pStream->Read((char *)&m_iMaxCurrency, sizeof(DWORD));
	Ctx.pStream->Read((char *)&m_iReplenishCurrency, sizeof(DWORD));

	Ctx.pStream->Read((char *)&dwLoad, sizeof(DWORD));
	if (dwLoad > 0)
		{
		m_List.InsertEmpty(dwLoad);

		for (i = 0; i < m_List.GetCount(); i++)
			{
			SCommodityDesc &Commodity = m_List[i];

			Commodity.sID.ReadFromStream(Ctx.pStream);

			Ctx.pStream->Read((char *)&dwLoad, sizeof(DWORD));
			Commodity.pItemType = g_pUniverse->FindItemType(dwLoad);

			CString sCriteria;
			sCriteria.ReadFromStream(Ctx.pStream);
			CItem::ParseCriteria(sCriteria, &Commodity.ItemCriteria);

			if (Ctx.dwVersion >= 62)
				{
				Commodity.PriceAdj.ReadFromStream(Ctx);
				Commodity.InventoryAdj.ReadFromStream(Ctx);
				}
			else
				{
				Ctx.pStream->Read((char *)&dwLoad, sizeof(DWORD));
				Commodity.PriceAdj.SetInteger(dwLoad);
				}

			Ctx.pStream->Read((char *)&Commodity.dwFlags, sizeof(DWORD));

			//	For now we don't support inventory adj in dynamic trade descs
			
			Commodity.dwFlags &= ~FLAG_INVENTORY_ADJ;
			}
		}
	}

void CTradingDesc::RefreshInventory (CSpaceObject *pObj, int iPercent)

//	RefreshInventory
//
//	Refresh the inventory for all entries that have an inventory
//	adjustment factor.

	{
	int i, j;
	bool bCargoChanged = false;

	for (i = 0; i < m_List.GetCount(); i++)
		if (m_List[i].dwFlags & FLAG_INVENTORY_ADJ)
			{
			//	Make a list of all item types that match the given
			//	criteria.

			TArray<CItemType *> ItemTable;
			for (j = 0; j < g_pUniverse->GetItemTypeCount(); j++)
				{
				CItemType *pType = g_pUniverse->GetItemType(j);
				CItem theItem(pType, 1);
				if (theItem.MatchesCriteria(m_List[i].ItemCriteria))
					ItemTable.Insert(pType);
				}

			//	Loop over the count

			if (ItemTable.GetCount() == 0)
				continue;

			//	Loop over all items refreshing them

			for (j = 0; j < ItemTable.GetCount(); j++)
				if (iPercent == 100 || mathRandom(1, 100) <= iPercent)
					{
					if (SetInventoryCount(pObj, m_List[i], ItemTable[j]))
						bCargoChanged = true;
					}
			}

	if (bCargoChanged)
		pObj->ItemsModified();
	}

bool CTradingDesc::Sells (CSpaceObject *pObj, const CItem &Item, int *retiPrice)

//	Sells
//
//	Returns TRUE if the given object can currently sell the given item type.
//	Optionally returns a price

	{
	int i;

	//	Loop over the commodity list and find the first entry that matches

	for (i = 0; i < m_List.GetCount(); i++)
		if (Matches(Item, m_List[i])
				&& (m_List[i].dwFlags & FLAG_SELLS))
			{
			int iPrice = ComputePrice(pObj, Item, m_List[i]);
			if (iPrice <= 0)
				return false;

			//	Done

			if (retiPrice)
				*retiPrice = iPrice;

			return true;
			}

	return false;
	}

bool CTradingDesc::SetInventoryCount (CSpaceObject *pObj, SCommodityDesc &Desc, CItemType *pItemType)

//	SetInventoryCount
//
//	Sets the count for the given item
//	Returns TRUE if the count was changed; FALSE otherwise.

	{
	bool bCargoChanged = false;
	CItemListManipulator ItemList(pObj->GetItemList());

	//	Roll number appearing

	int iItemCount = pItemType->GetNumberAppearing().Roll();

	//	Adjust based on inventory adjustment

	iItemCount = iItemCount * Desc.InventoryAdj.EvalAsInteger(pObj) / 100;

	//	If the item exists, set the count

	if (ItemList.SetCursorAtItem(CItem(pItemType, 1)))
		{
		ItemList.SetCountAtCursor(iItemCount);
		bCargoChanged = true;
		}

	//	Otherwise, add the appropriate number of items

	else if (iItemCount > 0)
		{
		ItemList.AddItem(CItem(pItemType, iItemCount));
		bCargoChanged = true;
		}

	//	Done

	return bCargoChanged;
	}

void CTradingDesc::WriteToStream (IWriteStream *pStream)

//	WriteToStream
//
//	Writes to a stream
//
//	DWORD			m_pCurrency UNID
//	DWORD			m_iMaxCurrency
//	DWORD			m_iReplenishCurrency
//	DWORD			No of orders
//
//	CString			sID
//	DWORD			pItemType
//	CString			ItemCriteria
//	CFormulaText	PriceAdj
//	CFormulaText	InventoryAdj
//	DWORD			dwFlags

	{
	int i;
	DWORD dwSave;

	dwSave = (m_pCurrency ? m_pCurrency->GetUNID() : 0);
	pStream->Write((char *)&dwSave, sizeof(DWORD));

	pStream->Write((char *)&m_iMaxCurrency, sizeof(DWORD));
	pStream->Write((char *)&m_iReplenishCurrency, sizeof(DWORD));

	dwSave = m_List.GetCount();
	pStream->Write((char *)&dwSave, sizeof(DWORD));

	for (i = 0; i < m_List.GetCount(); i++)
		{
		const SCommodityDesc &Commodity = m_List[i];

		Commodity.sID.WriteToStream(pStream);

		dwSave = (Commodity.pItemType ? Commodity.pItemType->GetUNID() : 0);
		pStream->Write((char *)&dwSave, sizeof(DWORD));

		CString sCriteria = CItem::GenerateCriteria(Commodity.ItemCriteria);
		sCriteria.WriteToStream(pStream);

		Commodity.PriceAdj.WriteToStream(pStream);
		Commodity.InventoryAdj.WriteToStream(pStream);

		pStream->Write((char *)&Commodity.dwFlags, sizeof(DWORD));
		}
	}
