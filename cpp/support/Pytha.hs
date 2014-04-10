module Pytha where

import qualified Data.Map.Strict as M
import Data.Ord
import Data.List
import Data.Functor

type SingleMap = M.Map Double Double
type TwoMap = M.Map (Double, Double) Double
type TwoRow = ((Double, Double), Double)

type DecompData = (SingleMap, SingleMap, TwoMap)

makeTwoMap :: SingleMap -> SingleMap -> TwoMap
makeTwoMap m1 m2 = M.fromList [((k1, k2), sqrt (a1 * a1 + a2 * a2)) | (k1, a1) <- M.toList m1, (k2, a2) <- M.toList m2]

buildDiffs :: DecompData -> TwoMap
buildDiffs (ls, ns, lns) = M.intersectionWith (curry $ abs . uncurry (-)) (makeTwoMap ls ns) lns

maxRelDiff :: TwoMap -> TwoMap -> TwoRow
maxRelDiff diffMap srcMap = M.foldrWithKey (\p v s -> maximumBy (comparing snd) [s, (p, v)]) ((0, 0), 0) $ M.intersectionWith (/) diffMap srcMap

maxRelDiff' m@(_, _, lns) = maxRelDiff (buildDiffs m) lns

avgRelDiff :: TwoMap -> TwoMap -> Double
avgRelDiff diffMap srcMap = (/ (fromIntegral $ M.size diffMap)) $ M.foldr (+) 0 $ M.intersectionWith (/) diffMap srcMap

avgRelDiff' m@(_, _, lns) = avgRelDiff (buildDiffs m) lns


medRelDiff :: TwoMap -> TwoMap -> Double
medRelDiff diffMap srcMap = sorted !! (length sorted `div` 2)
    where sorted = sort $ M.elems $ M.intersectionWith (/) diffMap srcMap

medRelDiff' m@(_, _, lns) = medRelDiff (buildDiffs m) lns

parseLines :: [[String]] -> DecompData
parseLines = foldr upstate (M.empty, M.empty, M.empty) . ((<$>) . (<$>)) read
    where upstate (x:0:a:[]) (ls, ns, lns) = (M.insert x a ls, ns, lns)
          upstate (0:x:a:[]) (ls, ns, lns) = (ls, M.insert x a ns, lns)
          upstate (x:y:a:[]) (ls, ns, lns) = (ls, ns, M.insert (x, y) a lns)

loadFile :: String -> IO [[String]]
loadFile name = do
    file <- readFile name
    return $ filter (not . null) $ map words $ lines file

getData :: String -> IO DecompData
getData name = loadFile name >>= (return . parseLines)

getStats name = loadFile name >>= (return . parseLines) >>= (\coe -> return ("Average: ", avgRelDiff' coe, "; median: ", medRelDiff' coe, "; maximum: ", maxRelDiff' coe))
