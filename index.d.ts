/**
 * Default type for keys in an sblgArticle.
 */
type sblgKeysDefault = Record<string, string>;

/**
 * An sblg article.  The passed-in Type gives structure to the "keys"
 * property.
 */
export interface sblgArticle<Type> 
{
	/**
	 * Source filename.
	 */
	src: string,
	/**
	 * "src" without suffix.
	 */
	base: string,
	/**
	 * "base" without suffix
	 */
	stripbase: string,
	/**
	 * "stripbase" without language suffix.
	 */
	striplangbase: string,
	/**
	 * Date of publication (epoch).
	 */
	time: number,
	/**
	 * Title content.
	 */
	title: {
		/**
		 * Stripped of HTML tags.
		 */
		text: string,
		/**
		 * With its HTML tags.
		 */
		xml: string,
	},
	/**
	 * Aside content.
	 */
	aside: {
		/**
		 * Stripped of HTML tags.
		 */
		text: string,
		/**
		 * With its HTML tags.
		 */
		xml: string,
	},
	/**
	 * Author content.
	 */
	author: {
		/**
		 * Stripped of HTML tags.
		 */
		text: string,
		/**
		 * With its HTML tags.
		 */
		xml: string,
	},
	/**
	 * Article content.
	 */
	article: {
		/**
		 * With its HTML tags.
		 */
		xml: string,
	},
	/**
	 * Variables set in the article.
	 */
	keys: Type,
	/**
	 * Tags set in the article.
	 */
	tags: string[],
}

/**
 * Article with default "keys" type.
 */
export interface sblgArticleDefault extends sblgArticle<sblgKeysDefault>
{
}

/**
 * All articles in the set with default typing for the articles' keys.
 */
export interface sblgDefault extends sblg<sblgKeysDefault>
{
}

/**
 * All articles in the set with explicit typing for the articles' keys.
 */
export interface sblg<Type> 
{
	/**
	 * Version of sblg producing the JSON.
	 */
	version: string,
	/**
	 * Array of articles.
	 */
	articles: sblgArticle<Type>[],
}
