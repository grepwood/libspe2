<!-- 
-->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">
	<xsl:output method="text" version="1.0" indent="yes" standalone="yes" />

<xsl:template match="/">
	<xsl:apply-templates select="//sectiondef"/>
</xsl:template>

<xsl:template match="sectiondef">
	<xsl:if test="@kind='func'">
		<xsl:text>
</xsl:text>
		<xsl:value-of select="../compoundname/text()"/> 
  		<xsl:apply-templates/>
	</xsl:if>
</xsl:template>

<xsl:template match="memberdef">
	<xsl:if test="@kind='function'">
  		<xsl:apply-templates select="definition" />
  	     	<xsl:apply-templates select="argsstring" />
		<xsl:text>;
</xsl:text>
	</xsl:if>
</xsl:template>

<xsl:template match="compoundname | includes "/>

</xsl:stylesheet>

