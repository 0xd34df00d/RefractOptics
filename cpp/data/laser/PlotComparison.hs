{-# LANGUAGE ViewPatterns #-}

import System.Environment
import Data.Monoid
import Data.List
import qualified Graphics.Gnuplot.Advanced as GA
import qualified Graphics.Gnuplot.Display as GD
import qualified Graphics.Gnuplot.Terminal as GT
import qualified Graphics.Gnuplot.Terminal.X11 as GTX
import qualified Graphics.Gnuplot.Terminal.PostScript as GTPS
import qualified Graphics.Gnuplot.Terminal.PNG as GTP
import qualified Graphics.Gnuplot.Plot.TwoDimensional as GP
import qualified Graphics.Gnuplot.Graph.TwoDimensional as GG
import qualified Graphics.Gnuplot.Value.Atom as GVA
import qualified Graphics.Gnuplot.Value.Tuple as GVT
import qualified Graphics.Gnuplot.LineSpecification as GLS
import qualified Graphics.Gnuplot.Frame.OptionSet as GFOS
import qualified Graphics.Gnuplot.Frame.Option as GFO
import qualified Graphics.Gnuplot.Frame as GF
import qualified GHC.IO.Exception as IOEx

data ParamsV a = ParamsV { p1 :: a, p2 :: a, p3 :: a } deriving (Show, Eq, Ord)
data Run a = Run { cnt :: Int, classic :: ParamsV a, modified :: ParamsV a } deriving (Show, Eq, Ord)

parseLine :: Read a => String -> Run a
parseLine (words -> (cnt:ws)) = Run (read cnt) (rp $ take 3 ws) (rp $ drop 3 ws)
    where rp (map read -> [p1', p2', p3']) = ParamsV p1' p2' p3'

filterPts :: (Ord b) => [(a, b)] -> [(a, b)]
filterPts pts = filter ((<= θ) . snd) pts
    where θ = sort (map snd pts) !! floor (fromIntegral (length pts) * 0.98)

gfxPrim :: (GVA.C a, GVT.C a, Ord a, Show a, Fractional a) => [Run a] -> Int -> (ParamsV a -> a) -> GF.T (GG.T Int a)
gfxPrim runs i f = GF.cons frameOpts $ single ("{/Symbol w}@^0_" ++ show i) classic <> single ("{/Symbol w}_" ++ show i) modified
    where single n p = GG.lineSpec (GLS.title n GLS.deflt) <$> GP.list GG.lines (map (\run -> (cnt run, f $ p run)) runs)
          frameOpts = GFOS.add (GFO.yRange "yrange") ["[:" ++ show (yquantile * 1.25) ++ "]"] $ GFOS.xLabel "l" $ GFOS.yLabel "{/Symbol d}" GFOS.deflt
          yquantile = sort (map f $ map classic runs ++ map modified runs) !! floor (fromIntegral (2 * length runs) * 0.98)

plotWTerm :: (GD.C gfx) => String -> String -> gfx -> IO IOEx.ExitCode
plotWTerm "x11" _     = GA.plot GTX.cons
plotWTerm "eps" fname = GA.plot $ GTPS.color $ GTPS.eps $ GTPS.cons $ fname ++ ".eps"
plotWTerm "png" fname = GA.plot $ GTP.trueColor $ GTP.cons $ fname ++ ".png"

process :: String -> String -> IO ()
process term fname = do
    runs <- (map parseLine . filter ((/= '#') . head) . lines) <$> readFile fname :: IO [Run Double]
    mapM_ (\(p', n) -> (plotWTerm term (fname ++ "_parameter" ++ show n) $ gfxPrim runs n p')) $ zip [p1, p2, p3] [1..]

main :: IO ()
main = do
    [mode, file] <- getArgs
    process mode file
