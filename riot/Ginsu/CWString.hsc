{-# OPTIONS -fglasgow-exts -ffi  -#include <wchar.h> #-}
-- arch-tag: 72067bff-05e1-4c0e-94aa-34b54f437d92

module Ginsu.CWString (
    -- utf8 versions
    withUTF8String,
    withUTF8StringLen,
    newUTF8String,
    newUTF8StringLen,
    peekUTF8String,
    peekUTF8StringLen,
    -- wchar stuff
    withCWString,
--    withCWStringLen,
    newCWString,
    newCWStringLen,
    peekCWString,
--    peekCWStringLen,
    wcharIsUnicode,
    CWChar, 
    CWString, 
    CWStringLen,
    -- locale versions 
    withLCString,
    withLCStringLen,
    newLCString,
    newLCStringLen,
    peekLCStringLen,
    peekLCString,
    charIsRepresentable

    ) where

import Data.Bits
import Foreign.C.String(withCString,CString,CStringLen,CWString,CWStringLen)
import Foreign.C.Types
import Char
import Foreign
import Monad
import qualified CForeign
import GHC.Exts
import IO


-- #ifndef CONFIG_INCLUDED
-- #define CONFIG_INCLUDED
-- #include <config.h>
-- #endif
#include <wchar.h>
#include <limits.h>
#include <stdlib.h>


type CWChar = CWchar
--type CWChar = (#type wchar_t)
--type CWString = Ptr CWChar
--type CWStringLen = (CWString, Int)


fi x = fromIntegral x

-------------------
-- CWChar functions
-------------------

{-# INLINE wcharIsUnicode #-}
wcharIsUnicode :: Bool

wcharIsUnicode = True

-- support functions 
wNUL :: CWChar
wNUL = 0
pairLength :: String -> CString -> CStringLen
pairLength  = flip (,) . length

cwCharsToChars :: [CWChar] -> [Char]
cwCharsToChars xs  = map castCWCharToChar xs
charsToCWChars :: [Char] -> [CWChar]
charsToCWChars xs  = map castCharToCWChar xs

castCWCharToChar :: CWChar -> Char
castCWCharToChar ch = chr (fromIntegral ch )

castCharToCWChar :: Char -> CWChar
castCharToCWChar ch = fromIntegral (ord ch)


-- exported functions
peekCWString    :: CWString -> IO String
peekCWString cp = loop 0
  where
    loop i = do
        val <- peekElemOff cp i
        if val == wNUL then return [] else do
            rest <- loop (i+1)
            return (castCWCharToChar val : rest)

--peekCWStringLen           :: CWStringLen -> IO String
--peekCWStringLen (cp, len) = loop 0
--  where
--    loop i | i == len  = return []
--	   | otherwise = do
--        	val <- peekElemOff cp i
--        	rest <- loop (i+1)
--        	return (castCWCharToChar val : rest)

newCWString :: String -> IO CWString
newCWString str = do
  ptr <- mallocArray0 (length str)
  let
	go [] n##   = pokeElemOff ptr (I## n##) wNUL
    	go (c:cs) n## = do pokeElemOff ptr (I## n##) (castCharToCWChar c); go cs (n## +## 1##)
  go str 0##
  return ptr

newCWStringLen     :: String -> IO CWStringLen
newCWStringLen str = do
  ptr <- mallocArray0 len
  let
	go [] n##     = return ()
    	go (c:cs) n## = do pokeElemOff ptr (I## n##) (castCharToCWChar c); go cs (n## +## 1##)
  go str 0##
  return (ptr, len)
  where
    len = length str

withCWString :: String -> (CWString -> IO a) -> IO a
withCWString str f =
  allocaArray0 (length str) $ \ptr ->
      let
	go [] n##     = pokeElemOff ptr (I## n##) wNUL
    	go (c:cs) n## = do pokeElemOff ptr (I## n##) (castCharToCWChar c); go cs (n## +## 1##)
      in do
      go str 0##
      f ptr

--withCWStringLen         :: String -> (CWStringLen -> IO a) -> IO a
--withCWStringLen str f =
--  allocaArray len $ \ptr ->
--      let
--	go [] n##     = return ()
--    	go (c:cs) n## = do pokeElemOff ptr (I## n##) (castCharToCWChar c); go cs (n## +## 1##)
--      in do
--      go str 0##
--      f (ptr,len)
--  where
--    len = length str

newtype MBState = MBState { _mbstate :: (Ptr MBState)}

withMBState :: (MBState -> IO a) -> IO a
withMBState act = allocaBytes (#const sizeof(mbstate_t)) (\mb -> c_memset mb 0 (#const sizeof(mbstate_t)) >> act (MBState mb)) 

clearMBState :: MBState -> IO ()
clearMBState (MBState mb) = c_memset mb 0 (#const sizeof(mbstate_t)) >> return ()




wcsrtombs :: CWString -> (CString, CSize) -> IO CSize
wcsrtombs wcs (cs,len) = alloca (\p -> poke p wcs >> withMBState (\mb -> wcsrtombs' p cs len mb)) where
    wcsrtombs'  p cs len mb = c_wcsrtombs cs p len mb >>= \x -> case x of 
        -1 -> do
            sp <- peek p 
            poke sp ((fi (ord '?'))::CWChar)
            poke p wcs
            clearMBState mb
            wcsrtombs' p cs len mb
        _ -> return x

#def inline HsInt hs_get_mb_cur_max () { return MB_CUR_MAX; }
foreign import ccall unsafe hs_get_mb_cur_max :: IO Int 

mb_cur_max :: Int
mb_cur_max = unsafePerformIO hs_get_mb_cur_max 


charIsRepresentable :: Char -> IO Bool
charIsRepresentable ch = fmap (/= -1) $ allocaBytes mb_cur_max (\cs -> c_wctomb cs (fi $ ord ch)) 

foreign import ccall unsafe "stdlib.h wctomb" c_wctomb :: CString -> CWChar -> IO CInt
foreign import ccall unsafe "stdlib.h wcsrtombs" c_wcsrtombs :: CString -> (Ptr (Ptr CWChar)) -> CSize -> MBState -> IO CSize
foreign import ccall unsafe "string.h memset" c_memset :: Ptr a -> CInt -> CSize -> IO (Ptr a)

foreign import ccall unsafe "stdlib.h mbstowcs" c_mbstowcs :: CWString -> CString -> CSize -> IO CSize

mbstowcs a b s = throwIf (== -1) (const "mbstowcs") $ c_mbstowcs a b s 


peekLCString    :: CString -> IO String
peekLCString cp  = do 
    sz <- mbstowcs nullPtr cp 0
    allocaArray (fi $ sz + 1) (\wcp -> mbstowcs wcp cp (sz + 1) >> peekCWString wcp)



-- TODO fix for embeded NULs
peekLCStringLen           :: CStringLen -> IO String
peekLCStringLen (cp, len)  =  allocaBytes (len + 1) $ \ncp -> do
    copyBytes ncp cp len
    pokeElemOff ncp len 0
    peekLCString ncp
    

newLCString :: String -> IO CString
newLCString s = withCWString s $ \wcs -> do mallocArray0 alen >>= \cs -> wcsrtombs wcs (cs, fi alen) >> return cs where
    alen = mb_cur_max * length s
                               

newLCStringLen     :: String -> IO CStringLen
newLCStringLen str  = newLCString str >>= \cs -> return (pairLength1 str cs)

withLCString :: String -> (CString -> IO a) -> IO a
withLCString s a = withCWString s $ \wcs -> allocaArray0 alen (\cs -> wcsrtombs wcs (cs,fi alen) >> a cs) where
    alen = mb_cur_max * length s

withLCStringLen         :: String -> (CStringLen -> IO a) -> IO a
withLCStringLen s a = withCWString s $ \wcs -> allocaArray0 alen (\cs -> wcsrtombs wcs (cs,fi alen) >>= \sz -> a (cs,fi sz)) where
    alen = mb_cur_max * length s


pairLength1 :: String -> CString -> CStringLen
pairLength1  = flip (,) . length


-----------------
-- UTF8 versions
-----------------


withUTF8String :: String -> (CString -> IO a) -> IO a
withUTF8String hsStr = CForeign.withCString (toUTF hsStr)

withUTF8StringLen :: String -> (CStringLen -> IO a) -> IO a
withUTF8StringLen hsStr = CForeign.withCStringLen (toUTF hsStr)

newUTF8String :: String -> IO CString
newUTF8String = CForeign.newCString . toUTF

newUTF8StringLen :: String -> IO CStringLen
newUTF8StringLen = CForeign.newCStringLen . toUTF

peekUTF8String :: CString -> IO String
peekUTF8String strPtr = fmap fromUTF $ CForeign.peekCString strPtr

peekUTF8StringLen :: CStringLen -> IO String
peekUTF8StringLen strPtr = fmap fromUTF $ CForeign.peekCStringLen strPtr


-- these should read and write directly from/to memory.
-- A first pass will be needed to determine the size of the allocated region

toUTF :: String -> String
toUTF [] = []
toUTF (x:xs) | ord x<=0x007F = x:toUTF xs
	     | ord x<=0x07FF = chr (0xC0 .|. ((ord x `shift` (-6)) .&. 0x1F)):
			       chr (0x80 .|. (ord x .&. 0x3F)):
			       toUTF xs
	     | otherwise     = chr (0xE0 .|. ((ord x `shift` (-12)) .&. 0x0F)):
			       chr (0x80 .|. ((ord x `shift` (-6)) .&. 0x3F)):
			       chr (0x80 .|. (ord x .&. 0x3F)):
			       toUTF xs

fromUTF :: String -> String
fromUTF [] = []
fromUTF (all@(x:xs)) | ord x<=0x7F = x:fromUTF xs
		     | ord x<=0xBF = err
		     | ord x<=0xDF = twoBytes all
		     | ord x<=0xEF = threeBytes all
		     | otherwise   = err
  where
    twoBytes (x1:x2:xs) = chr (((ord x1 .&. 0x1F) `shift` 6) .|.
			       (ord x2 .&. 0x3F)):fromUTF xs
    twoBytes _ = error "fromUTF: illegal two byte sequence"

    threeBytes (x1:x2:x3:xs) = chr (((ord x1 .&. 0x0F) `shift` 12) .|.
				    ((ord x2 .&. 0x3F) `shift` 6) .|.
				    (ord x3 .&. 0x3F)):fromUTF xs
    threeBytes _ = error "fromUTF: illegal three byte sequence" 
    
    err = error "fromUTF: illegal UTF-8 character"


