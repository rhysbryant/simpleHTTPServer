"""
generates a header file with all of the files matching the pattern for embedding them in a binary
additionally an index is created

author Rhys Bryant
license public domain 
"""

import os
import sys
import os
from glob import glob
import mimetypes
import gzip

MIN_FILE_SIZE_TO_GZIP = 40000

class FileEntry:
  def __init__(self,symName,path,gZipped):
    self.symName = symName
    self.path = path
    self.gZipped = gZipped

def escapeSymbolName(value:str):
  return value.replace("\\","_").replace(".","_").replace("/","_").replace(" ","_").replace("-","_").replace("+","_")

def outputFile(srcFileName,symName):
  ba = bytearray(open(srcFileName,"rb").read())
  wasGzipped = False

  if len(ba) > MIN_FILE_SIZE_TO_GZIP:
    ba = gzip.compress(ba)
    wasGzipped = True

  print("static const char %s[] = {"%(symName))
  index = 0
  for byte in ba:
    if index >0:
      sys.stdout.write(",")
    sys.stdout.write("0x%.2x"%(byte))

    if((index % 22) ==0):
      sys.stdout.write("\n")
    index+=1

  sys.stdout.write("};\n")

  return wasGzipped

def getMimetype(fileName):
  mimeType,_ = mimetypes.guess_type(fileName.replace("\\","/"))
  if mimeType is None:
    mimeType = "text/plan"

  symName = "file_content_type_"+ escapeSymbolName(mimeType)

  return symName,mimeType

def printFileDump(pattern):
  """
  return is generated symbol name to file path dict
  """
  filesAdded = []
  #generate hex arrays for files
  #for root, dir, files in os.walk(dirPath):
  for part in pattern.split(" "):
    for items in glob(part): #fnmatch.filter(files, "*.*"):
          
        path = items
        symName = escapeSymbolName(path)
        wasGzipped = outputFile(path,symName)
        filesAdded.append(FileEntry(symName,path.replace("\\","/"),wasGzipped))
  return filesAdded

def printFileIndex(filesAdded: list[FileEntry]):
  flags = {
    "file_content_type_mask":31,
    "file_content_reserved_flag":64,
    "file_content_gzipped":128,
  }
  flagCounter = 0
  
  structDef="""
/* 

file index section 

*/
struct FileContent{
  const char* content;
  const int length;
  const char* fileName; 
  const char flags;
};
struct FileContentMIMEType{
 const char* name;
 const char nameLength;
};
"""
  sys.stdout.write(structDef)
  first = True

  strFileIndex ="\nFileContent files[]={\n"
  strFileContentTypeMapping = "\nFileContentMIMEType filesType[]={\n"

  for k in filesAdded:
      if(not first):
          strFileIndex += ","
      first = False

      mimetypeFlagName,value = getMimetype(k.path)
      fileFlags = mimetypeFlagName

      if not mimetypeFlagName in flags:
        flags[mimetypeFlagName] = flagCounter
        flagCounter += 1
        if flagCounter >1:
          strFileContentTypeMapping+=","

        strFileContentTypeMapping += "{\"%s\",sizeof(\"%s\") - 1}\n"%(value,value)

      if k.gZipped:
        fileFlags +="|file_content_gzipped"
      
      if k.path == "index.html":
        k.path = ""

      strFileIndex += "{%s,sizeof(%s),\"%s\",%s}\n"%(k.symName,k.symName,"/"+k.path,fileFlags)
  
  strFileIndex += "};"
  strFileContentTypeMapping += "};"
  
  flagDef = ""
  for k in flags.keys():
    flagDef += "const char %s = %d;\n"%(k,flags[k])

  sys.stdout.write(flagDef)
  sys.stdout.write(strFileContentTypeMapping)
  sys.stdout.write(strFileIndex)

def main():
  mimetypes.add_type('application/javascript', '.js', strict=True)
  fileHeader = """

//contents generated by %s any manual changes will be overwritten 

"""%(" ".join(sys.argv))
  print(fileHeader)
  filesAdded = printFileDump(sys.argv[1])

  #build an index of the files dumped above
  printFileIndex(filesAdded)
 
main()
