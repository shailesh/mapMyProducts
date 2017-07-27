 #include "headers.h"
0006 
0007 
0008 
0009 
0010 
0011 
0012 
0013 
0014 
0015 
0016 //

0017 // Global state variables

0018 //

0019 
0020 //// later figure out how these are persisted

0021 map<uint256, CProduct> mapMyProducts;
0022 
0023 
0024 
0025 
0026 map<uint256, CProduct> mapProducts;
0027 CCriticalSection cs_mapProducts;
0028 
0029 bool AdvertInsert(const CProduct& product)
0030 {
0031     uint256 hash = product.GetHash();
0032     bool fNew = false;
0033     bool fUpdated = false;
0034 
0035     CRITICAL_BLOCK(cs_mapProducts)
0036     {
0037         // Insert or find existing product

0038         pair<map<uint256, CProduct>::iterator, bool> item = mapProducts.insert(make_pair(hash, product));
0039         CProduct* pproduct = &(*(item.first)).second;
0040         fNew = item.second;
0041 
0042         // Update if newer

0043         if (product.nSequence > pproduct->nSequence)
0044         {
0045             *pproduct = product;
0046             fUpdated = true;
0047         }
0048     }
0049 
0050     //if (fNew)

0051     //    NotifyProductAdded(hash);

0052     //else if (fUpdated)

0053     //    NotifyProductUpdated(hash);

0054 
0055     return (fNew || fUpdated);
0056 }
0057 
0058 void AdvertErase(const CProduct& product)
0059 {
0060     uint256 hash = product.GetHash();
0061     CRITICAL_BLOCK(cs_mapProducts)
0062         mapProducts.erase(hash);
0063     //NotifyProductDeleted(hash);

0064 }
0065 
0066 
0067 
0068 
0069 
0070 
0071 
0072 
0073 
0074 
0075 
0076 
0077 
0078 
0079 
0080 
0081 
0082 
0083 
0084 template<typename T>
0085 unsigned int Union(T& v1, T& v2)
0086 {
0087     // v1 = v1 union v2

0088     // v1 and v2 must be sorted

0089     // returns the number of elements added to v1

0090 
0091     ///// need to check that this is equivalent, then delete this comment

0092     //vector<unsigned short> vUnion(v1.size() + v2.size());

0093     //vUnion.erase(set_union(v1.begin(), v1.end(),

0094     //                       v2.begin(), v2.end(),

0095     //                       vUnion.begin()),

0096     //             vUnion.end());

0097 
0098     T vUnion;
0099     vUnion.reserve(v1.size() + v2.size());
0100     set_union(v1.begin(), v1.end(),
0101               v2.begin(), v2.end(),
0102               back_inserter(vUnion));
0103     unsigned int nAdded = vUnion.size() - v1.size();
0104     if (nAdded > 0)
0105         v1 = vUnion;
0106     return nAdded;
0107 }
0108 
0109 void CUser::AddAtom(unsigned short nAtom, bool fOrigin)
0110 {
0111     // Ignore duplicates

0112     if (binary_search(vAtomsIn.begin(), vAtomsIn.end(), nAtom) ||
0113         find(vAtomsNew.begin(), vAtomsNew.end(), nAtom) != vAtomsNew.end())
0114         return;
0115 
0116     //// instead of zero atom, should change to free atom that propagates,

0117     //// limited to lower than a certain value like 5 so conflicts quickly

0118     // The zero atom never propagates,

0119     // new atoms always propagate through the user that created them

0120     if (nAtom == 0 || fOrigin)
0121     {
0122         vector<unsigned short> vTmp(1, nAtom);
0123         Union(vAtomsIn, vTmp);
0124         if (fOrigin)
0125             vAtomsOut.push_back(nAtom);
0126         return;
0127     }
0128 
0129     vAtomsNew.push_back(nAtom);
0130 
0131     if (vAtomsNew.size() >= nFlowthroughRate || vAtomsOut.empty())
0132     {
0133         // Select atom to flow through to vAtomsOut

0134         vAtomsOut.push_back(vAtomsNew[GetRand(vAtomsNew.size())]);
0135 
0136         // Merge vAtomsNew into vAtomsIn

0137         sort(vAtomsNew.begin(), vAtomsNew.end());
0138         Union(vAtomsIn, vAtomsNew);
0139         vAtomsNew.clear();
0140     }
0141 }
0142 
0143 bool AddAtomsAndPropagate(uint256 hashUserStart, const vector<unsigned short>& vAtoms, bool fOrigin)
0144 {
0145     CReviewDB reviewdb;
0146     map<uint256, vector<unsigned short> > pmapPropagate[2];
0147     pmapPropagate[0][hashUserStart] = vAtoms;
0148 
0149     for (int side = 0; !pmapPropagate[side].empty(); side = 1 - side)
0150     {
0151         map<uint256, vector<unsigned short> >& mapFrom = pmapPropagate[side];
0152         map<uint256, vector<unsigned short> >& mapTo = pmapPropagate[1 - side];
0153 
0154         for (map<uint256, vector<unsigned short> >::iterator mi = mapFrom.begin(); mi != mapFrom.end(); ++mi)
0155         {
0156             const uint256& hashUser = (*mi).first;
0157             const vector<unsigned short>& vReceived = (*mi).second;
0158 
0159             ///// this would be a lot easier on the database if it put the new atom at the beginning of the list,

0160             ///// so the change would be right next to the vector size.

0161 
0162             // Read user

0163             CUser user;
0164             reviewdb.ReadUser(hashUser, user);
0165             unsigned int nIn = user.vAtomsIn.size();
0166             unsigned int nNew = user.vAtomsNew.size();
0167             unsigned int nOut = user.vAtomsOut.size();
0168 
0169             // Add atoms received

0170             foreach(unsigned short nAtom, vReceived)
0171                 user.AddAtom(nAtom, fOrigin);
0172             fOrigin = false;
0173 
0174             // Don't bother writing to disk if no changes

0175             if (user.vAtomsIn.size() == nIn && user.vAtomsNew.size() == nNew)
0176                 continue;
0177 
0178             // Propagate

0179             if (user.vAtomsOut.size() > nOut)
0180                 foreach(const uint256& hash, user.vLinksOut)
0181                     mapTo[hash].insert(mapTo[hash].end(), user.vAtomsOut.begin() + nOut, user.vAtomsOut.end());
0182 
0183             // Write back

0184             if (!reviewdb.WriteUser(hashUser, user))
0185                 return false;
0186         }
0187         mapFrom.clear();
0188     }
0189     return true;
0190 }
0191 
0192 
0193 
0194 
0195 
0196 
0197 bool CReview::AcceptReview()
0198 {
0199     // Timestamp

0200     nTime = GetTime();
0201 
0202     // Check signature

0203     if (!CKey::Verify(vchPubKeyFrom, GetSigHash(), vchSig))
0204         return false;
0205 
0206     CReviewDB reviewdb;
0207 
0208     // Add review text to recipient

0209     vector<CReview> vReviews;
0210     reviewdb.ReadReviews(hashTo, vReviews);
0211     vReviews.push_back(*this);
0212     if (!reviewdb.WriteReviews(hashTo, vReviews))
0213         return false;
0214 
0215     // Add link from sender

0216     CUser user;
0217     uint256 hashFrom = Hash(vchPubKeyFrom.begin(), vchPubKeyFrom.end());
0218     reviewdb.ReadUser(hashFrom, user);
0219     user.vLinksOut.push_back(hashTo);
0220     if (!reviewdb.WriteUser(hashFrom, user))
0221         return false;
0222 
0223     reviewdb.Close();
0224 
0225     // Propagate atoms to recipient

0226     vector<unsigned short> vZeroAtom(1, 0);
0227     if (!AddAtomsAndPropagate(hashTo, user.vAtomsOut.size() ? user.vAtomsOut : vZeroAtom, false))
0228         return false;
0229 
0230     return true;
0231 }
0232 
0233 
0234 
0235 
0236 
0237 bool CProduct::CheckSignature()
0238 {
0239     return (CKey::Verify(vchPubKeyFrom, GetSigHash(), vchSig));
0240 }
0241 
0242 bool CProduct::CheckProduct()
0243 {
0244     if (!CheckSignature())
0245         return false;
0246 
0247     // Make sure it's a summary product

0248     if (!mapDetails.empty() || !vOrderForm.empty())
0249         return false;
0250 
0251     // Look up seller's atom count

0252     CReviewDB reviewdb("r");
0253     CUser user;
0254     reviewdb.ReadUser(GetUserHash(), user);
0255     nAtoms = user.GetAtomCount();
0256     reviewdb.Close();
0257 
0258     ////// delme, this is now done by AdvertInsert

0259     //// Store to memory

0260     //CRITICAL_BLOCK(cs_mapProducts)

0261     //    mapProducts[GetHash()] = *this;

0262 
0263     return true;
0264 }
