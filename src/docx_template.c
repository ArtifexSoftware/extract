/* THIS IS AUTO-GENERATED CODE, DO NOT EDIT. */

#include "docx_template.h"

char extract_docx_word_document_xml[] = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
                "<w:document xmlns:o=\"urn:schemas-microsoft-com:office:office\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\" xmlns:v=\"urn:schemas-microsoft-com:vml\" xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\" xmlns:w10=\"urn:schemas-microsoft-com:office:word\" xmlns:wp=\"http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing\" xmlns:wps=\"http://schemas.microsoft.com/office/word/2010/wordprocessingShape\" xmlns:wpg=\"http://schemas.microsoft.com/office/word/2010/wordprocessingGroup\" xmlns:mc=\"http://schemas.openxmlformats.org/markup-compatibility/2006\" xmlns:wp14=\"http://schemas.microsoft.com/office/word/2010/wordprocessingDrawing\" xmlns:w14=\"http://schemas.microsoft.com/office/word/2010/wordml\" mc:Ignorable=\"w14 wp14\"><w:body><w:p><w:pPr><w:pStyle w:val=\"Normal\"/><w:bidi w:val=\"0\"/><w:jc w:val=\"left\"/><w:rPr></w:rPr></w:pPr><w:r><w:rPr></w:rPr></w:r></w:p><w:sectPr><w:type w:val=\"nextPage\"/><w:pgSz w:w=\"12240\" w:h=\"15840\"/><w:pgMar w:left=\"1134\" w:right=\"1134\" w:header=\"0\" w:top=\"1134\" w:footer=\"0\" w:bottom=\"1134\" w:gutter=\"0\"/><w:pgNumType w:fmt=\"decimal\"/><w:formProt w:val=\"false\"/><w:textDirection w:val=\"lrTb\"/></w:sectPr></w:body></w:document>";
int  extract_docx_word_document_xml_len = sizeof(extract_docx_word_document_xml) - 1;

int extract_docx_write(extract_zip_t* zip, const char* word_document_xml, int word_document_xml_length)
{
    int e = -1;
    {
        char text[] = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\"><Default Extension=\"xml\" ContentType=\"application/xml\"/><Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/><Default Extension=\"png\" ContentType=\"image/png\"/><Default Extension=\"jpeg\" ContentType=\"image/jpeg\"/><Override PartName=\"/_rels/.rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/><Override PartName=\"/docProps/core.xml\" ContentType=\"application/vnd.openxmlformats-package.core-properties+xml\"/><Override PartName=\"/docProps/app.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.extended-properties+xml\"/><Override PartName=\"/word/_rels/document.xml.rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/><Override PartName=\"/word/document.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml\"/><Override PartName=\"/word/styles.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.styles+xml\"/><Override PartName=\"/word/settings.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.settings+xml\"/><Override PartName=\"/word/fontTable.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.fontTable+xml\"/>\n"
                "</Types>"
                ;
        if (extract_zip_write_file(zip, text, sizeof(text)-1, "[Content_Types].xml")) goto end;
    }
    
    {
        char text[] = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\"><Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/package/2006/relationships/metadata/core-properties\" Target=\"docProps/core.xml\"/><Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/extended-properties\" Target=\"docProps/app.xml\"/><Relationship Id=\"rId3\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"word/document.xml\"/>\n"
                "</Relationships>"
                ;
        if (extract_zip_write_file(zip, text, sizeof(text)-1, "_rels/.rels")) goto end;
    }
    
    {
        char text[] = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
                "<Properties xmlns=\"http://schemas.openxmlformats.org/officeDocument/2006/extended-properties\" xmlns:vt=\"http://schemas.openxmlformats.org/officeDocument/2006/docPropsVTypes\"><Template></Template><TotalTime>0</TotalTime><Application>LibreOffice/6.4.3.2$OpenBSD_X86_64 LibreOffice_project/40$Build-2</Application><Pages>1</Pages><Words>0</Words><Characters>0</Characters><CharactersWithSpaces>0</CharactersWithSpaces><Paragraphs>0</Paragraphs></Properties>"
                ;
        if (extract_zip_write_file(zip, text, sizeof(text)-1, "docProps/app.xml")) goto end;
    }
    
    {
        char text[] = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
                "<cp:coreProperties xmlns:cp=\"http://schemas.openxmlformats.org/package/2006/metadata/core-properties\" xmlns:dc=\"http://purl.org/dc/elements/1.1/\" xmlns:dcterms=\"http://purl.org/dc/terms/\" xmlns:dcmitype=\"http://purl.org/dc/dcmitype/\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"><dcterms:created xsi:type=\"dcterms:W3CDTF\">2020-07-15T19:06:00Z</dcterms:created><dc:creator></dc:creator><dc:description></dc:description><dc:language>en-US</dc:language><cp:lastModifiedBy></cp:lastModifiedBy><cp:revision>0</cp:revision><dc:subject></dc:subject><dc:title></dc:title></cp:coreProperties>"
                ;
        if (extract_zip_write_file(zip, text, sizeof(text)-1, "docProps/core.xml")) goto end;
    }
    
    if (word_document_xml) {
        if (extract_zip_write_file(zip, word_document_xml, word_document_xml_length, "word/document.xml")) goto end;
    }
    else {
        char text[] = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
                "<w:document xmlns:o=\"urn:schemas-microsoft-com:office:office\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\" xmlns:v=\"urn:schemas-microsoft-com:vml\" xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\" xmlns:w10=\"urn:schemas-microsoft-com:office:word\" xmlns:wp=\"http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing\" xmlns:wps=\"http://schemas.microsoft.com/office/word/2010/wordprocessingShape\" xmlns:wpg=\"http://schemas.microsoft.com/office/word/2010/wordprocessingGroup\" xmlns:mc=\"http://schemas.openxmlformats.org/markup-compatibility/2006\" xmlns:wp14=\"http://schemas.microsoft.com/office/word/2010/wordprocessingDrawing\" xmlns:w14=\"http://schemas.microsoft.com/office/word/2010/wordml\" mc:Ignorable=\"w14 wp14\"><w:body><w:p><w:pPr><w:pStyle w:val=\"Normal\"/><w:bidi w:val=\"0\"/><w:jc w:val=\"left\"/><w:rPr></w:rPr></w:pPr><w:r><w:rPr></w:rPr></w:r></w:p><w:sectPr><w:type w:val=\"nextPage\"/><w:pgSz w:w=\"12240\" w:h=\"15840\"/><w:pgMar w:left=\"1134\" w:right=\"1134\" w:header=\"0\" w:top=\"1134\" w:footer=\"0\" w:bottom=\"1134\" w:gutter=\"0\"/><w:pgNumType w:fmt=\"decimal\"/><w:formProt w:val=\"false\"/><w:textDirection w:val=\"lrTb\"/></w:sectPr></w:body></w:document>"
                ;
        if (extract_zip_write_file(zip, text, sizeof(text)-1, "word/document.xml")) goto end;
    }
    
    {
        char text[] = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
                "<w:fonts xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\"><w:font w:name=\"Times New Roman\"><w:charset w:val=\"00\"/><w:family w:val=\"roman\"/><w:pitch w:val=\"variable\"/></w:font><w:font w:name=\"Symbol\"><w:charset w:val=\"02\"/><w:family w:val=\"roman\"/><w:pitch w:val=\"variable\"/></w:font><w:font w:name=\"Arial\"><w:charset w:val=\"00\"/><w:family w:val=\"swiss\"/><w:pitch w:val=\"variable\"/></w:font><w:font w:name=\"Liberation Serif\"><w:altName w:val=\"Times New Roman\"/><w:charset w:val=\"01\"/><w:family w:val=\"roman\"/><w:pitch w:val=\"variable\"/></w:font><w:font w:name=\"Liberation Sans\"><w:altName w:val=\"Arial\"/><w:charset w:val=\"01\"/><w:family w:val=\"swiss\"/><w:pitch w:val=\"variable\"/></w:font></w:fonts>"
                ;
        if (extract_zip_write_file(zip, text, sizeof(text)-1, "word/fontTable.xml")) goto end;
    }
    
    {
        char text[] = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
                "<w:settings xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\"><w:zoom w:percent=\"100\"/><w:defaultTabStop w:val=\"709\"/><w:autoHyphenation w:val=\"true\"/></w:settings>"
                ;
        if (extract_zip_write_file(zip, text, sizeof(text)-1, "word/settings.xml")) goto end;
    }
    
    {
        char text[] = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
                "<w:styles xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\" xmlns:w14=\"http://schemas.microsoft.com/office/word/2010/wordml\" xmlns:mc=\"http://schemas.openxmlformats.org/markup-compatibility/2006\" mc:Ignorable=\"w14\"><w:docDefaults><w:rPrDefault><w:rPr><w:rFonts w:ascii=\"Liberation Serif\" w:hAnsi=\"Liberation Serif\" w:eastAsia=\"DejaVu Sans\" w:cs=\"DejaVu Sans\"/><w:kern w:val=\"2\"/><w:sz w:val=\"24\"/><w:szCs w:val=\"24\"/><w:lang w:val=\"en-US\" w:eastAsia=\"zh-CN\" w:bidi=\"hi-IN\"/></w:rPr></w:rPrDefault><w:pPrDefault><w:pPr><w:widowControl/><w:suppressAutoHyphens w:val=\"true\"/></w:pPr></w:pPrDefault></w:docDefaults><w:style w:type=\"paragraph\" w:styleId=\"Normal\"><w:name w:val=\"Normal\"/><w:qFormat/><w:pPr><w:widowControl/><w:bidi w:val=\"0\"/></w:pPr><w:rPr><w:rFonts w:ascii=\"Liberation Serif\" w:hAnsi=\"Liberation Serif\" w:eastAsia=\"DejaVu Sans\" w:cs=\"DejaVu Sans\"/><w:color w:val=\"auto\"/><w:kern w:val=\"2\"/><w:sz w:val=\"24\"/><w:szCs w:val=\"24\"/><w:lang w:val=\"en-US\" w:eastAsia=\"zh-CN\" w:bidi=\"hi-IN\"/></w:rPr></w:style><w:style w:type=\"paragraph\" w:styleId=\"Heading\"><w:name w:val=\"Heading\"/><w:basedOn w:val=\"Normal\"/><w:next w:val=\"TextBody\"/><w:qFormat/><w:pPr><w:keepNext w:val=\"true\"/><w:spacing w:before=\"240\" w:after=\"120\"/></w:pPr><w:rPr><w:rFonts w:ascii=\"Liberation Sans\" w:hAnsi=\"Liberation Sans\" w:eastAsia=\"DejaVu Sans\" w:cs=\"DejaVu Sans\"/><w:sz w:val=\"28\"/><w:szCs w:val=\"28\"/></w:rPr></w:style><w:style w:type=\"paragraph\" w:styleId=\"TextBody\"><w:name w:val=\"Body Text\"/><w:basedOn w:val=\"Normal\"/><w:pPr><w:spacing w:lineRule=\"auto\" w:line=\"276\" w:before=\"0\" w:after=\"140\"/></w:pPr><w:rPr></w:rPr></w:style><w:style w:type=\"paragraph\" w:styleId=\"List\"><w:name w:val=\"List\"/><w:basedOn w:val=\"TextBody\"/><w:pPr></w:pPr><w:rPr></w:rPr></w:style><w:style w:type=\"paragraph\" w:styleId=\"Caption\"><w:name w:val=\"Caption\"/><w:basedOn w:val=\"Normal\"/><w:qFormat/><w:pPr><w:suppressLineNumbers/><w:spacing w:before=\"120\" w:after=\"120\"/></w:pPr><w:rPr><w:i/><w:iCs/><w:sz w:val=\"24\"/><w:szCs w:val=\"24\"/></w:rPr></w:style><w:style w:type=\"paragraph\" w:styleId=\"Index\"><w:name w:val=\"Index\"/><w:basedOn w:val=\"Normal\"/><w:qFormat/><w:pPr><w:suppressLineNumbers/></w:pPr><w:rPr></w:rPr></w:style></w:styles>"
                ;
        if (extract_zip_write_file(zip, text, sizeof(text)-1, "word/styles.xml")) goto end;
    }
    
    {
        char text[] = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\"><Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" Target=\"styles.xml\"/><Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/fontTable\" Target=\"fontTable.xml\"/><Relationship Id=\"rId3\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/settings\" Target=\"settings.xml\"/>\n"
                "</Relationships>"
                ;
        if (extract_zip_write_file(zip, text, sizeof(text)-1, "word/_rels/document.xml.rels")) goto end;
    }
    
    e = 0;
    end:
    return e;
}
